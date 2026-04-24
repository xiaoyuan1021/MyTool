#pragma once

#include <QObject>
#include <QStringList>
#include <QTimer>
#include <QElapsedTimer>
#include "core/pipeline_manager.h"
#include "core/pipeline.h"
#include "ui/roi_manager.h"
#include "data/detection_result_report.h"

/**
 * 检测统计信息
 */
struct DetectionStats
{
    int totalCount = 0;         ///< 总图片数
    int processedCount = 0;     ///< 已处理数
    int passCount = 0;          ///< 合格数
    int failCount = 0;          ///< 不合格数
    qint64 elapsedMs = 0;       ///< 已用时间 (ms)

    /// 合格率 (%)
    double passRate() const
    {
        if (processedCount == 0) return 0.0;
        return static_cast<double>(passCount) / processedCount * 100.0;
    }

    /// 平均每张耗时 (ms)
    double avgTimePerImage() const
    {
        if (processedCount == 0) return 0.0;
        return static_cast<double>(elapsedMs) / processedCount;
    }

    /// 预估剩余时间 (ms)
    qint64 estimatedRemainingMs() const
    {
        if (processedCount == 0) return 0;
        int remaining = totalCount - processedCount;
        return static_cast<qint64>(avgTimePerImage() * remaining);
    }

    void reset()
    {
        totalCount = 0;
        processedCount = 0;
        passCount = 0;
        failCount = 0;
        elapsedMs = 0;
    }
};

/**
 * 单个检测任务（一张图片的检测）
 */
struct ImageDetectionTask
{
    QString imageId;            ///< 图片ID
    QString imagePath;          ///< 图片文件路径
    QString imageName;          ///< 图片名称
    QList<RoiConfig> roiConfigs; ///< 该图片的ROI配置列表
};

/**
 * 自动检测控制器（方案B：基于RoiManager中已有的图片和ROI配置）
 *
 * 工作流程：
 * 1. 从 RoiManager 获取所有已添加的图片及其 ROI 配置
 * 2. 对每张图片的每个 ROI 区域裁剪并执行 Pipeline 检测
 * 3. 根据 Pipeline 结果判断 OK/NG
 * 4. 实时统计合格/不合格数量
 * 5. 支持停止/暂停/恢复控制
 */
class AutoDetectionController : public QObject
{
    Q_OBJECT

public:
    explicit AutoDetectionController(
        PipelineManager* pipeline,
        RoiManager* roiManager,
        QObject* parent = nullptr);

    ~AutoDetectionController() = default;

    // ==================== 控制接口 ====================

    /// 开始批量检测（从RoiManager自动获取图片和ROI配置）
    void startDetection();

    /// 停止检测
    void stopDetection();

    /// 暂停检测
    void pauseDetection();

    /// 恢复检测
    void resumeDetection();

    // ==================== 状态查询 ====================

    bool isRunning() const { return m_running; }
    bool isPaused() const { return m_paused; }
    DetectionStats stats() const { return m_stats; }
    int currentIndex() const { return m_currentIndex; }

    /// 获取所有检测报告
    const QList<DetectionResultReport>& results() const { return m_reports; }

    /// 获取不合格图片列表
    QStringList failedImages() const { return m_failedImages; }

    /// 获取检测结果摘要
    QString summaryString() const;

signals:
    /// 检测开始
    void detectionStarted(int totalCount);

    /// 检测完成
    void detectionFinished(const DetectionStats& stats);

    /// 检测被停止
    void detectionStopped();

    /// 进度更新
    void progressUpdated(int currentIndex, int totalCount);

    /// 单张图片检测完成 (passed: 该图片所有ROI都通过才为true)
    void imageProcessed(int index, const QString& imagePath, bool passed);

    /// 统计信息更新
    void statsUpdated(const DetectionStats& stats);

    /// 日志消息
    void logMessage(const QString& message);

private:
    /// 构建检测任务列表（从RoiManager获取）
    QList<ImageDetectionTask> buildTaskList();

    /// 处理下一个任务
    void processNextTask();

    /// 处理单个图片任务（在后台线程执行）
    void processImageTask(const ImageDetectionTask& task);

    /// 完成检测
    void finishDetection();

    PipelineManager* m_pipeline;
    RoiManager* m_roiManager;

    QList<ImageDetectionTask> m_tasks;    ///< 待检测任务列表
    int m_currentIndex = 0;
    bool m_running = false;
    bool m_paused = false;
    bool m_stopRequested = false;

    DetectionStats m_stats;
    QList<DetectionResultReport> m_reports;
    QStringList m_failedImages;

    QElapsedTimer m_elapsedTimer;       ///< 计时器
};