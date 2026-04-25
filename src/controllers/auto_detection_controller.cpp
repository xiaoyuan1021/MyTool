#include "controllers/auto_detection_controller.h"
#include "logger.h"
#include "config/detection_config_types.h"
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
    emit logMessage(QString("[buildTaskList] 共有 %1 张图片").arg(imageIds.size()));

    for (const QString& imageId : imageIds) {
        ImageDetectionTask task;
        task.imageId = imageId;
        task.imagePath = m_roiManager->getImageFilePath(imageId);
        task.imageName = m_roiManager->getImageName(imageId);

        // 获取该图片的ROI配置（关键：使用 getRoiConfigsForImage，不会切换当前图片）
        task.roiConfigs = m_roiManager->getRoiConfigsForImage(imageId);

        emit logMessage(QString("[buildTaskList] 图片 '%1': 路径='%2', ROI配置数=%3")
            .arg(task.imageName).arg(task.imagePath).arg(task.roiConfigs.size()));

        // 输出每个ROI配置的详细信息
        for (int i = 0; i < task.roiConfigs.size(); ++i) {
            const RoiConfig& cfg = task.roiConfigs[i];
            emit logMessage(QString("  [ROI %1] id=%2, name=%3, rect=(%4,%5,%6,%7), active=%8, 检测项数=%9")
                .arg(i).arg(cfg.roiId).arg(cfg.roiName)
                .arg(cfg.roiRect.x()).arg(cfg.roiRect.y())
                .arg(cfg.roiRect.width()).arg(cfg.roiRect.height())
                .arg(cfg.isActive ? "true" : "false")
                .arg(cfg.detectionItems.size()));

            // 输出每个检测项信息
            for (int j = 0; j < cfg.detectionItems.size(); ++j) {
                const DetectionItem& det = cfg.detectionItems[j];
                emit logMessage(QString("    [DetItem %1] id=%2, name=%3, type=%4, enabled=%5, configisEmpty=%6")
                    .arg(j).arg(det.itemId).arg(det.itemName)
                    .arg(detectionTypeToString(det.type))
                    .arg(det.enabled ? "true" : "false")
                    .arg(det.config.isEmpty() ? "true" : "false"));
            }

            // 输出PipelineConfig关键参数
            const PipelineConfig& pc = cfg.pipelineConfig;
            emit logMessage(QString("    PipelineConfig: brightness=%1, contrast=%2, gamma=%3, sharpen=%4, channel=%5, grayLow=%6, grayHigh=%7")
                .arg(pc.brightness).arg(pc.contrast).arg(pc.gamma).arg(pc.sharpen)
                .arg(static_cast<int>(pc.channel)).arg(pc.grayLow).arg(pc.grayHigh));
        }

        // 至少需要图片文件路径
        if (task.imagePath.isEmpty()) {
            emit logMessage(QString("跳过图片 '%1': 无文件路径").arg(task.imageName));
            continue;
        }

        // 如果没有ROI配置，也跳过（全图检测的场景可以后续扩展）
        if (task.roiConfigs.isEmpty()) {
            emit logMessage(QString("跳过图片 '%1': 未配置ROI").arg(task.imageName));
            continue;
        }

        tasks.append(task);
    }

    emit logMessage(QString("[buildTaskList] 构建了 %1 个检测任务").arg(tasks.size()));
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
    PipelineManager* pipeline = m_pipeline;
    QList<RoiConfig> roiConfigs = task.roiConfigs;  // 值拷贝，线程安全
    QString imagePath = task.imagePath;
    QString imageName = task.imageName;
    QString imageId = task.imageId;
    int currentIndex = m_currentIndex;
    int totalCount = m_stats.totalCount;

    // 使用 QtConcurrent 在后台线程执行，避免阻塞 UI
    // 注意：lambda中无法使用emit logMessage（无this指针），使用Logger::instance()代替
    QFuture<PipelineContext> future = QtConcurrent::run(
        [pipeline, imagePath, roiConfigs]() -> PipelineContext {
            Logger::instance()->info(QString("[检测] 开始处理图片: %1").arg(imagePath));

            // 加载图片
            cv::Mat image = cv::imread(imagePath.toStdString());
            if (image.empty()) {
                Logger::instance()->error(QString("[检测] 无法加载图片: %1").arg(imagePath));
                PipelineContext emptyCtx;
                emptyCtx.pass = false;
                emptyCtx.reason = QString("无法加载图片: %1").arg(imagePath);
                return emptyCtx;
            }
            Logger::instance()->info(QString("[检测] 图片加载成功: %1x%2").arg(image.cols).arg(image.rows));

            // 保存PipelineManager当前全局配置（执行完毕后恢复）
            PipelineConfig savedGlobalConfig = pipeline->getConfigSnapshot();

            bool allPassed = true;
            QString failReason;

            Logger::instance()->info(QString("[检测] ROI配置数量: %1").arg(roiConfigs.size()));

            for (const RoiConfig& roiConfig : roiConfigs) {
                Logger::instance()->info(QString("[检测] 处理ROI: name=%1, active=%2, rect=(%3,%4,%5,%6), 检测项数=%7")
                    .arg(roiConfig.roiName)
                    .arg(roiConfig.isActive ? "true" : "false")
                    .arg(roiConfig.roiRect.x()).arg(roiConfig.roiRect.y())
                    .arg(roiConfig.roiRect.width()).arg(roiConfig.roiRect.height())
                    .arg(roiConfig.detectionItems.size()));

                if (!roiConfig.isActive) {
                    Logger::instance()->info(QString("[检测] ROI '%1' 未激活，跳过").arg(roiConfig.roiName));
                    continue;
                }

                cv::Mat roiImage;
                QRectF rect = roiConfig.roiRect;

                // 裁剪ROI区域
                if (!rect.isEmpty() && rect.width() > 0 && rect.height() > 0) {
                    int x = std::max(0, (int)std::floor(rect.x()));
                    int y = std::max(0, (int)std::floor(rect.y()));
                    int w = std::min((int)std::ceil(rect.width()), image.cols - x);
                    int h = std::min((int)std::ceil(rect.height()), image.rows - y);

                    Logger::instance()->info(QString("[检测] ROI裁剪: x=%1, y=%2, w=%3, h=%4, 图片尺寸=%5x%6")
                        .arg(x).arg(y).arg(w).arg(h).arg(image.cols).arg(image.rows));

                    if (w > 0 && h > 0) {
                        roiImage = image(cv::Rect(x, y, w, h)).clone();
                    } else {
                        Logger::instance()->warning("[检测] ROI裁剪尺寸无效，使用全图");
                        roiImage = image.clone();
                    }
                } else {
                    Logger::instance()->info("[检测] 无ROI区域，使用全图");
                    roiImage = image.clone();
                }

                Logger::instance()->info(QString("[检测] ROI图像尺寸: %1x%2").arg(roiImage.cols).arg(roiImage.rows));

                // 【Bug1修复】应用该ROI专属的Pipeline配置，通过execute参数传递

                Logger::instance()->info(QString("[检测] 应用PipelineConfig: brightness=%1, contrast=%2, gamma=%3, sharpen=%4, channel=%5")
                    .arg(roiConfig.pipelineConfig.brightness)
                    .arg(roiConfig.pipelineConfig.contrast)
                    .arg(roiConfig.pipelineConfig.gamma)
                    .arg(roiConfig.pipelineConfig.sharpen)
                    .arg(static_cast<int>(roiConfig.pipelineConfig.channel)));

                // 执行Pipeline（传入ROI专属配置，线程安全）
                Logger::instance()->info("[检测] 开始执行Pipeline...");
                PipelineContext ctx = pipeline->execute(roiImage, roiConfig.pipelineConfig);
                Logger::instance()->info(QString("[检测] Pipeline执行完成: currentRegions=%1, barcodeResults=%2, matchedLines=%3, totalLines=%4")
                    .arg(ctx.currentRegions)
                    .arg(ctx.barcodeResults.size())
                    .arg(ctx.matchedLineCount)
                    .arg(ctx.totalLineCount));

                // 根据DetectionItem的配置判断 pass/fail
                bool roiPassed = true;
                QString roiFailReason;

                for (const DetectionItem& detItem : roiConfig.detectionItems) {
                    if (!detItem.enabled) {
                        Logger::instance()->info(QString("[检测] 检测项 '%1' 未启用，跳过").arg(detItem.itemName));
                        continue;
                    }

                    Logger::instance()->info(QString("[检测] 评估检测项: name=%1, type=%2")
                        .arg(detItem.itemName).arg(detectionTypeToString(detItem.type)));

                    if (detItem.type == DetectionType::Blob) {
                        BlobAnalysisConfig blobConfig;
                        blobConfig.fromJson(detItem.config);

                        int currentCount = ctx.currentRegions;
                        int minCount = blobConfig.minBlobCount;
                        int maxCount = blobConfig.maxBlobCount;

                        Logger::instance()->info(QString("[检测] Blob判定: currentCount=%1, min=%2, max=%3")
                            .arg(currentCount).arg(minCount).arg(maxCount));

                        if (currentCount < minCount || currentCount > maxCount) {
                            roiPassed = false;
                            roiFailReason += QString("Blob数量%1, 要求[%2,%3]")
                                .arg(currentCount).arg(minCount).arg(maxCount);
                            Logger::instance()->warning(QString("[检测] Blob判定: NG! %1").arg(roiFailReason));
                        } else {
                            Logger::instance()->info("[检测] Blob判定: OK");
                        }
                    }
                    else if (detItem.type == DetectionType::Barcode) {
                        BarcodeRecognitionConfig barcodeConfig;
                        barcodeConfig.fromJson(detItem.config);

                        Logger::instance()->info(QString("[检测] 条码判定: barcodeResults数=%1, minConfidence=%2")
                            .arg(ctx.barcodeResults.size()).arg(barcodeConfig.minConfidence));

                        if (ctx.barcodeResults.isEmpty()) {
                            roiPassed = false;
                            roiFailReason += "未检测到条码";
                            Logger::instance()->warning("[检测] 条码判定: NG! 未检测到条码");
                        } else {
                            bool foundValid = false;
                            for (const BarcodeResult& br : ctx.barcodeResults) {
                                Logger::instance()->info(QString("[检测] 条码结果: type=%1, data=%2, quality=%3")
                                    .arg(br.type).arg(br.data).arg(br.quality));
                                if (br.quality >= barcodeConfig.minConfidence * 100.0) {
                                    foundValid = true;
                                    break;
                                }
                            }
                            if (!foundValid) {
                                roiPassed = false;
                                roiFailReason += QString("条码识别质量不足(共%1个)")
                                    .arg(ctx.barcodeResults.size());
                                Logger::instance()->warning(QString("[检测] 条码判定: NG! %1").arg(roiFailReason));
                            } else {
                                Logger::instance()->info("[检测] 条码判定: OK");
                            }
                        }
                    }
                    else if (detItem.type == DetectionType::Line) {
                        LineDetectionConfig lineConfig;
                        lineConfig.fromJson(detItem.config);

                        Logger::instance()->info(QString("[检测] 直线判定: matchedLineCount=%1, totalLineCount=%2")
                            .arg(ctx.matchedLineCount).arg(ctx.totalLineCount));

                        if (ctx.matchedLineCount == 0 && ctx.totalLineCount == 0) {
                            roiPassed = false;
                            roiFailReason += "未检测到直线";
                            Logger::instance()->warning("[检测] 直线判定: NG! 未检测到直线");
                        } else {
                            Logger::instance()->info("[检测] 直线判定: OK");
                        }
                    }
                }

                Logger::instance()->info(QString("[检测] ROI '%1' 最终结果: %2")
                    .arg(roiConfig.roiName).arg(roiPassed ? "PASS" : "FAIL"));

                if (!roiPassed) {
                    allPassed = false;
                    if (!failReason.isEmpty()) failReason += "; ";
                    failReason += QString("[%1] %2")
                        .arg(roiConfig.roiName)
                        .arg(roiFailReason.isEmpty() ? "NG" : roiFailReason);
                }
            }

            // 不再需要手动恢复配置，execute内部已处理

            Logger::instance()->info(QString("[检测] 整体结果: %1, reason=%2")
                .arg(allPassed ? "PASS" : "FAIL").arg(failReason.isEmpty() ? "无" : failReason));

            PipelineContext resultCtx;
            resultCtx.pass = allPassed;
            resultCtx.reason = failReason;
            return resultCtx;
        }
    );

    // 使用 watcher 等待结果（在主线程中处理）
    QFutureWatcher<PipelineContext>* watcher = new QFutureWatcher<PipelineContext>(this);
    connect(watcher, &QFutureWatcher<PipelineContext>::finished, this,
        [this, watcher, task, currentIndex, totalCount]() {
            PipelineContext ctx = watcher->result();
            watcher->deleteLater();

            // 更新统计
            m_stats.processedCount++;
            m_stats.elapsedMs = m_elapsedTimer.elapsed();

            bool passed = ctx.pass;
            if (passed) {
                m_stats.passCount++;
            } else {
                m_stats.failCount++;
                m_failedImages.append(task.imagePath);
            }

            // 生成检测报告
            DetectionResultReport report = DetectionResultReport::fromPipelineContext(
                ctx, task.imageName, QString(), task.imageName);
            report.imageId = task.imageId;
            m_reports.append(report);

            // 发送信号
            emit imageProcessed(currentIndex, task.imagePath, passed);
            emit progressUpdated(currentIndex + 1, totalCount);
            emit statsUpdated(m_stats);

            // 记录日志
            QString status = passed ? "PASS" : "FAIL";
            emit logMessage(QString("[%1/%2] %3 -> %4 (%5)")
                .arg(currentIndex + 1)
                .arg(totalCount)
                .arg(task.imageName)
                .arg(status)
                .arg(ctx.reason.isEmpty() ? "OK" : ctx.reason));

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