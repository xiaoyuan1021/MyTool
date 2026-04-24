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
 * 自动检测控制器
 *
 * 管理批量图片的自动检测流程：
 * - 支持从文件夹或图片列表启动批量检测
 * - 逐张执行 Pipeline 检测
 * - 实时统计合格/不合格数量和合格率
 * - 支持停止/暂停/恢复控制
 * - 生成检测结果报告（预留 MQTT 上报接口）
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

    /// 开始批量检测
    /// @param imagePaths 待检测的图片路径列表
    void startDetection(const QStringList& imagePaths);

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

    /// 获取不合格图片路径列表
    QStringList failedImages() const { return m_failedImages; }

    /// 获取检测结果摘要（用于日志/显示）
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

    /// 单张图片检测完成
    void imageProcessed(int index, const QString& imagePath, bool passed);

    /// 统计信息更新
    void statsUpdated(const DetectionStats& stats);

    /// 日志消息
    void logMessage(const QString& message);

private:
    /// 处理下一张图片
    void processNextImage();

    /// 处理单张图片
    void processSingleImage(const QString& imagePath);

    /// 完成检测
    void finishDetection();

    PipelineManager* m_pipeline;
    RoiManager* m_roiManager;

    QStringList m_imagePaths;           ///< 待检测图片列表
    int m_currentIndex = 0;
    bool m_running = false;
    bool m_paused = false;
    bool m_stopRequested = false;

    DetectionStats m_stats;
    QList<DetectionResultReport> m_reports;
    QStringList m_failedImages;

    QElapsedTimer m_elapsedTimer;       ///< 计时器
};