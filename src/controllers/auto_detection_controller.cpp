#include "controllers/auto_detection_controller.h"
#include "logger.h"
#include "utils/path_utils.h"
#include "config/detection_config_types.h"
#include "algorithm/image_utils.h"
#include "algorithm/detection_evaluator.h"
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

    for (const QString& imageId : imageIds) {
        ImageDetectionTask task;
        task.imageId = imageId;
        task.imagePath = m_roiManager->getImageFilePath(imageId);
        task.imageName = m_roiManager->getImageName(imageId);
        task.roiConfigs = m_roiManager->getRoiConfigsForImage(imageId);

        if (task.imagePath.isEmpty() || task.roiConfigs.isEmpty()) {
            continue;
        }

        tasks.append(task);
    }

    Logger::instance()->info(QString("[buildTaskList] 构建了 %1 个检测任务（共 %2 张图片）")
        .arg(tasks.size()).arg(imageIds.size()));
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

            cv::Mat image = PathUtils::readImageFromFile(imagePath, cv::IMREAD_COLOR);
            if (image.empty()) {
                Logger::instance()->error(QString("[检测] 无法加载图片: %1").arg(imagePath));
                imageResult.passed = false;
                imageResult.failReason = QString("无法加载图片: %1").arg(imagePath);
                return imageResult;
            }

            // 遍历每个ROI，独立评估检测结果
            for (const RoiConfig& roiConfig : roiConfigs) {
                if (!roiConfig.isActive) continue;

                cv::Rect r = ImageUtils::mapRoiToCvRect(roiConfig.roiRect, image.cols, image.rows);
                cv::Mat roiImage = r.empty() ? image.clone() : image(r).clone();

                PipelineContext ctx = pipelinePtr->execute(roiImage, roiConfig.pipelineConfig);

                // ★ 使用DetectionEvaluator评估该ROI的所有检测项
                RoiDetectionResult roiResult = DetectionEvaluator::evaluateRoi(
                    roiConfig, ctx, roiImage, tabMgrPtr);
                roiResult.imageId = imageId;

                imageResult.addRoiResult(roiResult);
            }

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

            // ★ 生成检测报告，包含每个ROI的检测项结果
            DetectionResultReport report;
            report.imageId = task.imageId;
            report.imageName = task.imageName;
            report.passed = imageResult.passed;
            report.failReason = imageResult.failReason;

            // 汇总所有ROI的检测项结果
            for (const auto& roiResult : imageResult.roiResults) {
                for (const auto& itemResult : roiResult.itemResults) {
                    DetectionItemReport itemReport;
                    itemReport.itemName = itemResult.itemName;
                    itemReport.detectionType = itemResult.detectionType;
                    itemReport.passed = itemResult.passed;
                    itemReport.failReason = itemResult.failReason;
                    report.itemResults.append(itemReport);
                }
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
