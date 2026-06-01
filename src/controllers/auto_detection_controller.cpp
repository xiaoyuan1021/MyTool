#include "controllers/auto_detection_controller.h"
#include "logger.h"
#include "utils/path_utils.h"
#include "config/detection_config_types.h"
#include "algorithm/image_utils.h"
#include "widgets/object_detection_tab_widget.h"
#include "widgets/tab_manager.h"
#include "data/roi_detection_result.h"
#include <QFileInfo>
#include <QCoreApplication>
#include <QtConcurrent/QtConcurrent>
#include <QFutureWatcher>

AutoDetectionController::AutoDetectionController(
    PipelineManager* pipeline,
    RoiManager* roiManager,
    QObject* parent)
    : QObject(parent)
    , m_pipeline(pipeline)
    , m_roiManager(roiManager)
{
}

// ==================== 控制接口 ====================

void AutoDetectionController::startDetection()
{
    if (m_running) {
        emit logMessage("检测已在运行中，请先停止当前检测");
        return;
    }

    // 从 RoiManager 构建任务列表
    m_tasks = buildTaskList();

    if (m_tasks.isEmpty()) {
        emit logMessage("没有要检测的图片，请先添加图片并配置ROI和检测项");
        return;
    }

    // 重置状态
    m_currentIndex = 0;
    m_stopRequested = false;
    m_paused = false;
    m_running = true;

    m_stats.reset();
    m_stats.totalCount = m_tasks.size();
    m_reports.clear();
    m_failedImages.clear();

    m_elapsedTimer.start();

    emit detectionStarted(m_stats.totalCount);
    emit logMessage(QString("开始批量检测: 共 %1 张图片").arg(m_stats.totalCount));

    // 开始处理第一个任务
    processNextTask();
}

void AutoDetectionController::stopDetection()
{
    if (!m_running) {
        return;
    }

    m_stopRequested = true;
    m_paused = false;
    emit logMessage("正在停止检测...");
}

void AutoDetectionController::pauseDetection()
{
    if (!m_running || m_paused) {
        return;
    }

    m_paused = true;
    emit logMessage("检测已暂停");
}

void AutoDetectionController::resumeDetection()
{
    if (!m_running || !m_paused) {
        return;
    }

    m_paused = false;
    emit logMessage("检测已恢复");

    // 继续处理下一个任务
    processNextTask();
}

// ==================== 状态查询 ====================

QString AutoDetectionController::summaryString() const
{
    return QString("总计: %1 | 已处理: %2 | 合格: %3 | 不合格: %4 | 合格率: %5% | 耗时: %6s | 平均: %7ms/张")
        .arg(m_stats.totalCount)
        .arg(m_stats.processedCount)
        .arg(m_stats.passCount)
        .arg(m_stats.failCount)
        .arg(m_stats.passRate(), 0, 'f', 1)
        .arg(m_stats.elapsedMs / 1000.0, 0, 'f', 1)
        .arg(m_stats.avgTimePerImage(), 0, 'f', 0);
}

// ==================== 内部实现 ====================

QList<ImageDetectionTask> AutoDetectionController::buildTaskList()
{
    QList<ImageDetectionTask> tasks;

    QStringList imageIds = m_roiManager->getImageIds();
    Logger::instance()->debug(QString("[buildTaskList] 共有 %1 张图片").arg(imageIds.size()));

    for (const QString& imageId : imageIds) {
        ImageDetectionTask task;
        task.imageId = imageId;
        task.imagePath = m_roiManager->getImageFilePath(imageId);
        task.imageName = m_roiManager->getImageName(imageId);

        // 获取该图片的ROI配置（关键：使用 getRoiConfigsForImage，不会切换当前图片）
        task.roiConfigs = m_roiManager->getRoiConfigsForImage(imageId);

        Logger::instance()->debug(QString("[buildTaskList] 图片 '%1': 路径='%2', ROI配置数=%3")
            .arg(task.imageName).arg(task.imagePath).arg(task.roiConfigs.size()));

        // 输出每个ROI配置的详细信息
        for (int i = 0; i < task.roiConfigs.size(); ++i) {
            const RoiConfig& cfg = task.roiConfigs[i];
            Logger::instance()->debug(QString("  [ROI %1] id=%2, name=%3, rect=(%4,%5,%6,%7), active=%8, 检测项数=%9")
                .arg(i).arg(cfg.roiId).arg(cfg.roiName)
                .arg(cfg.roiRect.x()).arg(cfg.roiRect.y())
                .arg(cfg.roiRect.width()).arg(cfg.roiRect.height())
                .arg(cfg.isActive ? "true" : "false")
                .arg(cfg.detectionItems.size()));

            // 输出每个检测项信息
            for (int j = 0; j < cfg.detectionItems.size(); ++j) {
                const DetectionItem& det = cfg.detectionItems[j];
                Logger::instance()->debug(QString("    [DetItem %1] id=%2, name=%3, type=%4, enabled=%5, configisEmpty=%6")
                    .arg(j).arg(det.itemId).arg(det.itemName)
                    .arg(detectionTypeToString(det.type))
                    .arg(det.enabled ? "true" : "false")
                    .arg(det.config.isEmpty() ? "true" : "false"));
            }

            // 输出PipelineConfig关键参数
            const PipelineConfig& pc = cfg.pipelineConfig;
            Logger::instance()->debug(QString("    PipelineConfig: brightness=%1, contrast=%2, gamma=%3, sharpen=%4, channel=%5, grayLow=%6, grayHigh=%7")
                .arg(pc.enhance.brightness).arg(pc.enhance.contrast).arg(pc.enhance.gamma).arg(pc.enhance.sharpen)
                .arg(static_cast<int>(pc.colorFilter.channel)).arg(pc.colorFilter.grayLow).arg(pc.colorFilter.grayHigh));
        }

        // 至少需要图片文件路径
        if (task.imagePath.isEmpty()) {
            Logger::instance()->debug(QString("跳过图片 '%1': 无文件路径").arg(task.imageName));
            continue;
        }

        // 如果没有ROI配置，也跳过（全图检测的场景可以后续扩展）
        if (task.roiConfigs.isEmpty()) {
            Logger::instance()->debug(QString("跳过图片 '%1': 未配置ROI").arg(task.imageName));
            continue;
        }

        tasks.append(task);
    }

    Logger::instance()->debug(QString("[buildTaskList] 构建了 %1 个检测任务").arg(tasks.size()));
    return tasks;
}

void AutoDetectionController::processNextTask()
{
    // 检查是否需要停止
    if (m_stopRequested) {
        finishDetection();
        return;
    }

    // 检查是否暂停
    if (m_paused) {
        return;
    }

    // 检查是否处理完毕
    if (m_currentIndex >= m_tasks.size()) {
        finishDetection();
        return;
    }

    // 处理当前任务
    processImageTask(m_tasks[m_currentIndex]);
}

void AutoDetectionController::processImageTask(const ImageDetectionTask& task)
{
    // 捕获需要的指针和数据（按值），避免在后台线程中捕获 this
    QPointer<PipelineManager> pipelinePtr = m_pipeline;
    TabManager* tabMgrPtr = m_tabManager;
    QList<RoiConfig> roiConfigs = task.roiConfigs;  // 值拷贝，线程安全
    QString imagePath = task.imagePath;
    QString imageName = task.imageName;
    QString imageId = task.imageId;
    int currentIndex = m_currentIndex;
    int totalCount = m_stats.totalCount;

    // 使用 QtConcurrent 在后台线程执行，避免阻塞 UI
    // 注意：lambda中无法使用emit logMessage（无this指针），使用Logger::instance()代替
    QFuture<ImageDetectionResult> future = QtConcurrent::run(
        [pipelinePtr, tabMgrPtr, imagePath, imageName, imageId, roiConfigs]() -> ImageDetectionResult {
            // 创建图片级别的检测结果
            ImageDetectionResult imageResult;
            imageResult.imageId = imageId;
            imageResult.imageName = imageName;
            imageResult.imagePath = imagePath;

            try {
            // 【P0修复】检查PipelineManager是否已被释放
            if (!pipelinePtr) {
                Logger::instance()->error(QString("[检测] PipelineManager已被释放，跳过处理: %1").arg(imagePath));
                imageResult.passed = false;
                imageResult.failReason = QString("PipelineManager已被释放: %1").arg(imagePath);
                return imageResult;
            }

            Logger::instance()->debug(QString("[检测] 开始处理图片: %1").arg(imagePath));

            // 加载图片（使用 PathUtils 支持中文路径）
            cv::Mat image = PathUtils::readImageFromFile(imagePath, cv::IMREAD_COLOR);
            if (image.empty()) {
                Logger::instance()->error(QString("[检测] 无法加载图片: %1").arg(imagePath));
                imageResult.passed = false;
                imageResult.failReason = QString("无法加载图片: %1").arg(imagePath);
                return imageResult;
            }
            Logger::instance()->debug(QString("[检测] 图片加载成功: %1x%2").arg(image.cols).arg(image.rows));

            Logger::instance()->debug(QString("[检测] ROI配置数量: %1").arg(roiConfigs.size()));

            // 遍历每个ROI，独立评估检测结果
            for (const RoiConfig& roiConfig : roiConfigs) {
                Logger::instance()->debug(QString("[检测] 处理ROI: name=%1, active=%2, rect=(%3,%4,%5,%6), 检测项数=%7")
                    .arg(roiConfig.roiName)
                    .arg(roiConfig.isActive ? "true" : "false")
                    .arg(roiConfig.roiRect.x()).arg(roiConfig.roiRect.y())
                    .arg(roiConfig.roiRect.width()).arg(roiConfig.roiRect.height())
                    .arg(roiConfig.detectionItems.size()));

                if (!roiConfig.isActive) {
                    Logger::instance()->debug(QString("[检测] ROI '%1' 未激活，跳过").arg(roiConfig.roiName));
                    continue;
                }

                // 为每个ROI创建独立的检测结果
                RoiDetectionResult roiResult(roiConfig.roiId, roiConfig.roiName, imageId);

                cv::Mat roiImage;
                QRectF rect = roiConfig.roiRect;

                // 裁剪ROI区域
                cv::Rect r = ImageUtils::mapRoiToCvRect(rect, image.cols, image.rows);
                if (!r.empty()) {
                    Logger::instance()->debug(QString("[检测] ROI裁剪: x=%1, y=%2, w=%3, h=%4, 图片尺寸=%5x%6")
                        .arg(r.x).arg(r.y).arg(r.width).arg(r.height).arg(image.cols).arg(image.rows));
                    roiImage = image(r).clone();
                } else {
                    Logger::instance()->debug("[检测] 无ROI区域或尺寸无效，使用全图");
                    roiImage = image.clone();
                }

                Logger::instance()->debug(QString("[检测] ROI图像尺寸: %1x%2").arg(roiImage.cols).arg(roiImage.rows));

                // 应用该ROI专属的Pipeline配置，通过execute参数传递
                Logger::instance()->debug(QString("[检测] 应用PipelineConfig: brightness=%1, contrast=%2, gamma=%3, sharpen=%4, channel=%5")
                    .arg(roiConfig.pipelineConfig.enhance.brightness)
                    .arg(roiConfig.pipelineConfig.enhance.contrast)
                    .arg(roiConfig.pipelineConfig.enhance.gamma)
                    .arg(roiConfig.pipelineConfig.enhance.sharpen)
                    .arg(static_cast<int>(roiConfig.pipelineConfig.colorFilter.channel)));

                // 执行Pipeline（传入ROI专属配置，线程安全）
                Logger::instance()->debug("[检测] 开始执行Pipeline...");
                PipelineContext ctx = pipelinePtr->execute(roiImage, roiConfig.pipelineConfig);
                Logger::instance()->debug(QString("[检测] Pipeline执行完成: regionCount=%1, barcodeResults=%2, matchedLines=%3, totalLines=%4")
                    .arg(ctx.regionCount)
                    .arg(ctx.barcodeResults.size())
                    .arg(ctx.matchedLineCount)
                    .arg(ctx.totalLineCount));

                // ★ 关键修改：将Pipeline结果存储到ROI独立的检测结果中，不再累加到全局变量
                roiResult.regionCount = ctx.regionCount;
                roiResult.regionFeatures = ctx.regionFeatures;
                roiResult.barcodeResults = ctx.barcodeResults;
                roiResult.ocrText = ctx.ocrText;
                roiResult.ocrRegions = ctx.ocrRegions;
                roiResult.matchedLineCount = ctx.matchedLineCount;
                roiResult.totalLineCount = ctx.totalLineCount;

                // 根据DetectionItem的配置判断 pass/fail
                for (const DetectionItem& detItem : roiConfig.detectionItems) {
                    if (!detItem.enabled) {
                        Logger::instance()->debug(QString("[检测] 检测项 '%1' 未启用，跳过").arg(detItem.itemName));
                        continue;
                    }

                    Logger::instance()->debug(QString("[检测] 评估检测项: name=%1, type=%2")
                        .arg(detItem.itemName).arg(detectionTypeToString(detItem.type)));

                    // 创建检测项评估结果
                    DetectionItemResult itemResult(detItem.itemId, detItem.itemName, detectionTypeToString(detItem.type));

                    if (detItem.type == DetectionType::Blob) {
                        BlobAnalysisConfig blobConfig;
                        blobConfig.fromJson(detItem.config);

                        int currentCount = ctx.regionCount;
                        int minCount = blobConfig.minBlobCount;
                        int maxCount = blobConfig.maxBlobCount;

                        Logger::instance()->debug(QString("[检测] Blob判定: currentCount=%1, min=%2, max=%3")
                            .arg(currentCount).arg(minCount).arg(maxCount));

                        if (currentCount < minCount || currentCount > maxCount) {
                            itemResult.passed = false;
                            itemResult.failReason = QString("Blob数量%1, 要求[%2,%3]")
                                .arg(currentCount).arg(minCount).arg(maxCount);
                            Logger::instance()->warning(QString("[检测] Blob判定: NG! %1").arg(itemResult.failReason));
                        } else {
                            Logger::instance()->debug("[检测] Blob判定: OK");
                        }
                    }
                    else if (detItem.type == DetectionType::Barcode) {
                        BarcodeRecognitionConfig barcodeConfig;
                        barcodeConfig.fromJson(detItem.config);

                        Logger::instance()->debug(QString("[检测] 条码判定: barcodeResults数=%1, minConfidence=%2")
                            .arg(ctx.barcodeResults.size()).arg(barcodeConfig.minConfidence));

                        if (ctx.barcodeResults.isEmpty()) {
                            itemResult.passed = false;
                            itemResult.failReason = "未检测到条码";
                            Logger::instance()->warning("[检测] 条码判定: NG! 未检测到条码");
                        } else {
                            Logger::instance()->debug(QString("[检测] 条码结果: 共%1个")
                                .arg(ctx.barcodeResults.size()));
                            for (const BarcodeResult& br : ctx.barcodeResults) {
                                Logger::instance()->debug(QString("  -> type=%1, data=%2, quality=%3")
                                    .arg(br.type).arg(br.data).arg(br.quality));
                            }
                            Logger::instance()->debug("[检测] 条码判定: OK");
                        }
                    }
                    else if (detItem.type == DetectionType::Line) {
                        LineDetectionConfig lineConfig;
                        lineConfig.fromJson(detItem.config);

                        Logger::instance()->debug(QString("[检测] 直线判定: matchedLineCount=%1, totalLineCount=%2")
                            .arg(ctx.matchedLineCount).arg(ctx.totalLineCount));

                        if (ctx.matchedLineCount == 0 && ctx.totalLineCount == 0) {
                            itemResult.passed = false;
                            itemResult.failReason = "未检测到直线";
                            Logger::instance()->warning("[检测] 直线判定: NG! 未检测到直线");
                        } else {
                            Logger::instance()->debug("[检测] 直线判定: OK");
                        }
                    }
                    else if (detItem.type == DetectionType::ObjectDetection) {
                        ObjectDetectionConfig objConfig;
                        objConfig.fromJson(detItem.config);

                        // 总是执行目标检测
                        int detectedCount = 0;
                        auto* objTab = tabMgrPtr ? tabMgrPtr->getTabAs<ObjectDetectionTabWidget>("目标检测") : nullptr;
                        if (objTab) {
                            if (objTab->isModelLoaded()) {
                                std::vector<DetectionResult> detResults = objTab->runDetection(roiImage);
                                detectedCount = static_cast<int>(detResults.size());
                                Logger::instance()->debug(QString("[检测] 目标检测执行完成: 检测到%1个目标").arg(detectedCount));
                            } else {
                                Logger::instance()->warning("[检测] 目标检测模型未加载，跳过数量检查");
                            }
                        } else {
                            Logger::instance()->warning("[检测] 目标检测Tab未找到");
                        }

                        int expectedCount = objConfig.expectedCount;

                        Logger::instance()->debug(QString("[检测] 目标检测判定: detectedCount=%1, expectedCount=%2")
                            .arg(detectedCount).arg(expectedCount));

                        // 如果设置了期望数量，检查是否匹配
                        if (expectedCount > 0 && detectedCount != expectedCount) {
                            itemResult.passed = false;
                            itemResult.failReason = QString("检测到%1个目标, 期望%2个")
                                .arg(detectedCount).arg(expectedCount);
                            Logger::instance()->warning(QString("[检测] 目标检测判定: NG! %1").arg(itemResult.failReason));
                        } else {
                            Logger::instance()->debug("[检测] 目标检测判定: OK");
                        }
                    }
                    else if (detItem.type == DetectionType::Ocr) {
                        OcrDetectionConfig ocrConfig;
                        ocrConfig.fromJson(detItem.config);

                        QString recognizedText = ctx.ocrText.trimmed();
                        QString expectedText = ocrConfig.expectedText.trimmed();

                        Logger::instance()->debug(QString("[检测] OCR判定: recognizedText='%1', expectedText='%2'")
                            .arg(recognizedText).arg(expectedText));

                        if (expectedText.isEmpty()) {
                            // 未设置期望文本，只要识别到文字就算OK
                            if (recognizedText.isEmpty()) {
                                itemResult.passed = false;
                                itemResult.failReason = "未识别到文字";
                                Logger::instance()->warning("[检测] OCR判定: NG! 未识别到文字");
                            } else {
                                Logger::instance()->debug("[检测] OCR判定: OK（未设置期望文本）");
                            }
                        } else {
                            // 设置了期望文本，进行匹配
                            bool matched = false;
                            if (ocrConfig.matchExact) {
                                // 精确匹配
                                matched = (recognizedText == expectedText);
                            } else {
                                // 模糊匹配（包含即可）
                                matched = recognizedText.contains(expectedText);
                            }

                            if (!matched) {
                                itemResult.passed = false;
                                itemResult.failReason = QString("OCR文本'%1'不匹配'%2'")
                                    .arg(recognizedText.isEmpty() ? "(空)" : recognizedText)
                                    .arg(expectedText);
                                Logger::instance()->warning(QString("[检测] OCR判定: NG! %1").arg(itemResult.failReason));
                            } else {
                                Logger::instance()->debug("[检测] OCR判定: OK");
                            }
                        }
                    }

                    // ★ 关键修改：将检测项结果添加到ROI独立的检测结果中
                    roiResult.addItemResult(itemResult);
                }

                Logger::instance()->debug(QString("[检测] ROI '%1' 最终结果: %2")
                    .arg(roiConfig.roiName).arg(roiResult.passed ? "PASS" : "FAIL"));
                Logger::instance()->debug(QString("[检测] ROI '%1' 摘要: %2")
                    .arg(roiConfig.roiName).arg(roiResult.toSummaryString()));

                // ★ 关键修改：将ROI检测结果添加到图片级别的检测结果中，不再累加到全局变量
                imageResult.addRoiResult(roiResult);
            }

            Logger::instance()->debug(QString("[检测] 整体结果: %1, reason=%2")
                .arg(imageResult.passed ? "PASS" : "FAIL")
                .arg(imageResult.failReason.isEmpty() ? "无" : imageResult.failReason));
            Logger::instance()->debug(QString("[检测] 图片摘要: %1").arg(imageResult.toSummaryString()));

            return imageResult;
            } catch (const cv::Exception& ex) {
                Logger::instance()->error(QString("[检测] OpenCV错误: %1").arg(ex.what()));
                imageResult.passed = false;
                imageResult.failReason = QString("OpenCV错误: %1").arg(ex.what());
                return imageResult;
            } catch (const std::exception& ex) {
                Logger::instance()->error(QString("[检测] 异常: %1").arg(ex.what()));
                imageResult.passed = false;
                imageResult.failReason = QString("异常: %1").arg(ex.what());
                return imageResult;
            }
        }
    );

    // 使用 watcher 等待结果（在主线程中处理）
    QFutureWatcher<ImageDetectionResult>* watcher = new QFutureWatcher<ImageDetectionResult>(this);
    connect(watcher, &QFutureWatcher<ImageDetectionResult>::finished, this,
        [this, watcher, task, currentIndex, totalCount]() {
            ImageDetectionResult imageResult = watcher->result();
            watcher->deleteLater();

            // 更新统计
            m_stats.processedCount++;
            m_stats.elapsedMs = m_elapsedTimer.elapsed();

            bool passed = imageResult.passed;
            if (passed) {
                m_stats.passCount++;
            } else {
                m_stats.failCount++;
                m_failedImages.append(task.imagePath);
            }

            // ★ 关键修改：生成检测报告，包含每个ROI的独立结果
            DetectionResultReport report;
            report.imageId = task.imageId;
            report.passed = imageResult.passed;
            report.failReason = imageResult.failReason;

            // 汇总所有ROI的检测数据（用于上报，但内部评估已独立完成）
            for (const auto& roiResult : imageResult.roiResults) {
                report.totalRegionCount += roiResult.regionCount;
                report.regions.insert(report.regions.end(),
                    roiResult.regionFeatures.begin(), roiResult.regionFeatures.end());
                report.barcodeResults.insert(report.barcodeResults.end(),
                    roiResult.barcodeResults.begin(), roiResult.barcodeResults.end());
            }

            m_reports.append(report);

            // 发送信号
            emit imageProcessed(currentIndex, task.imagePath, passed);
            emit progressUpdated(currentIndex + 1, totalCount);
            emit statsUpdated(m_stats);

            // ★ 关键修改：记录日志，显示每个ROI的独立结果
            QString status = passed ? "PASS" : "FAIL";
            QString roiSummary;
            for (const auto& roiResult : imageResult.roiResults) {
                if (!roiResult.passed) {
                    if (!roiSummary.isEmpty()) roiSummary += "; ";
                    roiSummary += QString("%1:%2").arg(roiResult.roiName, roiResult.failReason);
                }
            }
            emit logMessage(QString("[%1/%2] %3 -> %4 (%5)")
                .arg(currentIndex + 1)
                .arg(totalCount)
                .arg(task.imageName)
                .arg(status)
                .arg(roiSummary.isEmpty() ? "OK" : roiSummary));

            // 处理下一个任务
            m_currentIndex++;
            processNextTask();
        }
    );

    watcher->setFuture(future);
}

void AutoDetectionController::finishDetection()
{
    m_running = false;
    m_paused = false;
    m_stopRequested = false;
    m_stats.elapsedMs = m_elapsedTimer.elapsed();

    if (m_currentIndex >= m_tasks.size()) {
        emit logMessage(QString("批量检测完成! %1").arg(summaryString()));
    } else {
        emit logMessage(QString("批量检测已停止! %1").arg(summaryString()));
        emit detectionStopped();
    }

    emit detectionFinished(m_stats);
}

void AutoDetectionController::setupUiConnections(
    QPushButton* startBtn, QPushButton* stopBtn,
    QStatusBar* statusBar, QLabel* statusLabel)
{
    // 日志消息
    connect(this, &AutoDetectionController::logMessage,
            this, [](const QString& msg) { Logger::instance()->info(msg); });

    // 检测开始：禁用开始按钮，启用停止按钮
    connect(this, &AutoDetectionController::detectionStarted,
            this, [=](int totalCount) {
        startBtn->setEnabled(false);
        stopBtn->setEnabled(true);
        statusBar->showMessage(QString("批量检测中... 0/%1").arg(totalCount));
    });

    // 检测完成：启用开始按钮，禁用停止按钮
    connect(this, &AutoDetectionController::detectionFinished,
            this, [=](const DetectionStats& stats) {
        startBtn->setEnabled(true);
        stopBtn->setEnabled(false);
        statusBar->showMessage(
            QString("检测完成 | 总计:%1 合格:%2 不合格:%3 合格率:%4%")
                .arg(stats.totalCount).arg(stats.passCount)
                .arg(stats.failCount).arg(stats.passRate(), 0, 'f', 1), 10000);
    });

    // 检测停止
    connect(this, &AutoDetectionController::detectionStopped,
            this, [=]() {
        startBtn->setEnabled(true);
        stopBtn->setEnabled(false);
        statusBar->showMessage("检测已停止", 5000);
    });

    // 进度更新
    connect(this, &AutoDetectionController::progressUpdated,
            this, [=](int current, int total) {
        statusBar->showMessage(QString("批量检测中... %1/%2").arg(current).arg(total));
    });

    // 统计更新
    connect(this, &AutoDetectionController::statsUpdated,
            this, [=](const DetectionStats& stats) {
        QString statusText = QString("已处理: %1 | 合格: %2 | 不合格: %3 | 合格率: %4%")
            .arg(stats.processedCount).arg(stats.passCount)
            .arg(stats.failCount).arg(stats.passRate(), 0, 'f', 1);
        statusLabel->setText(statusText);
    });
}
