#include "controllers/auto_detection_controller.h"
#include "logger.h"
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

        // 获取该图片的ROI配置（关键：使用 getRoiConfigsForImage，不会切换当前图片）
        task.roiConfigs = m_roiManager->getRoiConfigsForImage(imageId);

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
    QFuture<PipelineContext> future = QtConcurrent::run(
        [pipeline, imagePath, roiConfigs]() -> PipelineContext {
            // 加载图片
            cv::Mat image = cv::imread(imagePath.toStdString());
            if (image.empty()) {
                PipelineContext emptyCtx;
                emptyCtx.pass = false;
                emptyCtx.reason = QString("无法加载图片: %1").arg(imagePath);
                return emptyCtx;
            }

            // 如果有多个ROI，对每个ROI分别检测
            // 简化策略：取第一个ROI区域裁剪后执行Pipeline
            // 后续可以扩展为多ROI分别检测后汇总
            bool allPassed = true;
            QString failReason;

            for (const RoiConfig& roiConfig : roiConfigs) {
                if (!roiConfig.isActive) continue;

                cv::Mat roiImage;
                QRectF rect = roiConfig.roiRect;

                // 裁剪ROI区域
                if (!rect.isEmpty() && rect.width() > 0 && rect.height() > 0) {
                    int x = std::max(0, (int)std::floor(rect.x()));
                    int y = std::max(0, (int)std::floor(rect.y()));
                    int w = std::min((int)std::ceil(rect.width()), image.cols - x);
                    int h = std::min((int)std::ceil(rect.height()), image.rows - y);

                    if (w > 0 && h > 0) {
                        roiImage = image(cv::Rect(x, y, w, h)).clone();
                    } else {
                        roiImage = image.clone();
                    }
                } else {
                    // 无ROI配置，使用全图
                    roiImage = image.clone();
                }

                // 执行Pipeline
                PipelineContext ctx = pipeline->execute(roiImage);

                // 根据DetectionItem的配置判断 pass/fail
                bool roiPassed = true;
                QString roiFailReason;

                for (const DetectionItem& detItem : roiConfig.detectionItems) {
                    if (!detItem.enabled) continue;

                    if (detItem.type == DetectionType::Blob) {
                        // 解析BlobAnalysisConfig
                        BlobAnalysisConfig blobConfig;
                        blobConfig.fromJson(detItem.config);

                        int currentCount = ctx.currentRegions;
                        int minCount = blobConfig.minBlobCount;
                        int maxCount = blobConfig.maxBlobCount;

                        if (currentCount < minCount || currentCount > maxCount) {
                            roiPassed = false;
                            roiFailReason = QString("Blob数量%1, 要求[%2,%3]")
                                .arg(currentCount).arg(minCount).arg(maxCount);
                        }
                    }
                    // 其他检测类型（Line/Barcode等）后续可扩展
                }

                if (!roiPassed) {
                    allPassed = false;
                    if (!failReason.isEmpty()) failReason += "; ";
                    failReason += QString("[%1] %2")
                        .arg(roiConfig.roiName)
                        .arg(roiFailReason.isEmpty() ? "NG" : roiFailReason);
                }
            }

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