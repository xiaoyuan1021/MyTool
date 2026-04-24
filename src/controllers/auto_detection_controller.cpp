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

void AutoDetectionController::startDetection(const QStringList& imagePaths)
{
    if (m_running) {
        emit logMessage("检测已在运行中，请先停止当前检测");
        return;
    }

    if (imagePaths.isEmpty()) {
        emit logMessage("没有要检测的图片");
        return;
    }

    // 重置状态
    m_imagePaths = imagePaths;
    m_currentIndex = 0;
    m_stopRequested = false;
    m_paused = false;
    m_running = true;

    m_stats.reset();
    m_stats.totalCount = imagePaths.size();
    m_reports.clear();
    m_failedImages.clear();

    m_elapsedTimer.start();

    emit detectionStarted(m_stats.totalCount);
    emit logMessage(QString("开始批量检测: 共 %1 张图片").arg(m_stats.totalCount));

    // 开始处理第一张
    processNextImage();
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

    // 继续处理下一张
    processNextImage();
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

void AutoDetectionController::processNextImage()
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
    if (m_currentIndex >= m_imagePaths.size()) {
        finishDetection();
        return;
    }

    // 处理当前图片
    processSingleImage(m_imagePaths[m_currentIndex]);
}

void AutoDetectionController::processSingleImage(const QString& imagePath)
{
    // 捕获需要的指针（按值），避免在后台线程中捕获 this
    PipelineManager* pipeline = m_pipeline;

    // 使用 QtConcurrent 在后台线程执行，避免阻塞 UI
    QFuture<PipelineContext> future = QtConcurrent::run(
        [pipeline, imagePath]() -> PipelineContext {
            // 加载图片
            cv::Mat image = cv::imread(imagePath.toStdString());
            if (image.empty()) {
                PipelineContext emptyCtx;
                emptyCtx.pass = false;
                emptyCtx.reason = QString("无法加载图片: %1").arg(imagePath);
                return emptyCtx;
            }

            // 在后台线程中执行 Pipeline（只读取配置，不修改 UI）
            return pipeline->execute(image);
        }
    );

    // 使用 watcher 等待结果（在主线程中处理）
    QFutureWatcher<PipelineContext>* watcher = new QFutureWatcher<PipelineContext>(this);
    connect(watcher, &QFutureWatcher<PipelineContext>::finished, this, [this, watcher, imagePath]() {
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
            m_failedImages.append(imagePath);
        }

        // 生成检测报告
        DetectionResultReport report = DetectionResultReport::fromPipelineContext(
            ctx, QFileInfo(imagePath).fileName());
        m_reports.append(report);

        // 发送信号
        emit imageProcessed(m_currentIndex, imagePath, passed);
        emit progressUpdated(m_currentIndex + 1, m_stats.totalCount);
        emit statsUpdated(m_stats);

        // 记录日志
        QString status = passed ? "PASS" : "FAIL";
        emit logMessage(QString("[%1/%2] %3 -> %4 (%5)")
            .arg(m_currentIndex + 1)
            .arg(m_stats.totalCount)
            .arg(QFileInfo(imagePath).fileName())
            .arg(status)
            .arg(ctx.reason.isEmpty() ? "OK" : ctx.reason));

        // 处理下一张
        m_currentIndex++;
        processNextImage();
    });

    watcher->setFuture(future);
}

void AutoDetectionController::finishDetection()
{
    m_running = false;
    m_paused = false;
    m_stopRequested = false;
    m_stats.elapsedMs = m_elapsedTimer.elapsed();

    if (m_currentIndex >= m_imagePaths.size()) {
        emit logMessage(QString("批量检测完成! %1").arg(summaryString()));
    } else {
        emit logMessage(QString("批量检测已停止! %1").arg(summaryString()));
        emit detectionStopped();
    }

    emit detectionFinished(m_stats);
}