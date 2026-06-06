#pragma once

#include <QObject>
#include <QTimer>
#include <QThreadPool>
#include <QQueue>
#include <QMutex>
#include <QFutureWatcher>
#include <QAtomicInt>
#include "pipeline_request.h"
#include "pipeline_result.h"

class PipelineManager;

/**
 * Pipeline调度器 - 统一管理所有pipeline执行请求
 * 
 * 设计原则：
 * 1. 单一入口：所有pipeline请求都通过这个类提交
 * 2. 消抖合并：短时间内多个请求只执行最后一个
 * 3. 去重过滤：相同配置的请求不重复执行
 * 4. 优先级队列：高优先级请求优先执行
 * 
 * 数据流：
 *   submit() → 队列 → 消抖 → 去重 → 线程池执行 → finished信号
 */
class PipelineScheduler : public QObject
{
    Q_OBJECT

public:
    explicit PipelineScheduler(PipelineManager* pipeline, QObject* parent = nullptr);
    ~PipelineScheduler();

    // ========== 请求接口 ==========

    /**
     * 提交pipeline执行请求
     * @param image 输入图像
     * @param config Pipeline配置
     * @param priority 优先级（数值越大优先级越高）
     * @param caller 调用者标识（用于日志追踪）
     * @return 请求ID，可用于取消
     */
    qint64 submit(const cv::Mat& image, const PipelineConfig& config, int priority = 0, const QString& caller = {});

    /**
     * 提交pipeline执行请求（使用PipelineRequest对象）
     */
    qint64 submit(PipelineRequest request);

    /**
     * 取消指定请求
     */
    void cancel(qint64 requestId);

    /**
     * 取消所有待处理请求
     */
    void cancelAll();

    // ========== 配置 ==========

    /**
     * 设置消抖延迟（毫秒）
     * 在此期间的多个请求会被合并为最后一个
     * 默认：100ms
     */
    void setDebounceMs(int ms);

    /**
     * 设置最大队列长度
     * 超出时丢弃最旧的低优先级请求
     * 默认：10
     */
    void setMaxQueueSize(int size);

    /**
     * 设置是否启用去重
     * 启用时，相同配置的请求不会重复执行
     * 默认：true
     */
    void setDeduplicationEnabled(bool enabled);

    // ========== 状态查询 ==========

    bool isProcessing() const { return m_processing.loadAcquire(); }
    int pendingCount() const;
    qint64 lastRequestId() const { return m_lastExecutedId.loadAcquire(); }

signals:
    /**
     * Pipeline执行完成
     */
    void finished(const PipelineResult& result);

    /**
     * 队列状态变化
     */
    void queueChanged(int pendingCount);

    /**
     * 执行状态变化
     */
    void processingChanged(bool processing);

    /**
     * 请求被取消
     */
    void cancelled(qint64 requestId);

private slots:
    void onDebounceTimeout();
    void onTaskFinished();

private:
    void processNext();
    void executeRequest(const PipelineRequest& request);
    void emitQueueChanged();

    PipelineManager* m_pipeline;

    // 调度配置
    QTimer* m_debounceTimer;
    int m_debounceMs = 100;
    int m_maxQueueSize = 10;
    bool m_deduplicationEnabled = true;

    // 执行队列（按优先级排序）
    QQueue<PipelineRequest> m_queue;
    mutable QMutex m_queueMutex;

    // 状态标志
    QAtomicInt m_processing{0};
    QAtomicInt m_lastSubmittedId{0};
    QAtomicInt m_lastExecutedId{0};

    // 当前执行的任务
    QFutureWatcher<PipelineResult>* m_watcher = nullptr;
};
