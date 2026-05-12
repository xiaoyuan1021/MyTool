#pragma once

#include "pipeline.h"
#include "pipeline_steps.h"
#include "config/constants.h"

#include <QObject>
#include <QString>
#include <QMutex>
#include <QMutexLocker>
#include <QAtomicInt>

class ImageProcessor;

/**
 * Pipeline管理器 - 负责Pipeline的创建、配置和执行
 *
 * 线程模型：
 * - execute() 在后台线程调用，使用局部 PipelineContext，不共享可变状态
 * - m_config 仅在UI线程读写，无需加锁
 * - m_lastContext 由 m_contextMutex 保护，供UI线程读取上次结果
 */
class PipelineManager : public QObject
{
    Q_OBJECT

public:
    explicit PipelineManager(QObject* parent = nullptr);
    ~PipelineManager();

    // ========== 配置管理（仅UI线程调用）==========

    void resetEnhancement();

    /// 获取配置的可变引用（UI线程直接读写，替代大量 trivial setter）
    PipelineConfig& mutableConfig() { return m_config; }

    /// 获取配置的常量引用
    const PipelineConfig& config() const { return m_config; }

    // 获取当前配置的拷贝（用于并发快照）
    PipelineConfig getConfigSnapshot() const { return m_config; }

    // 设置配置（仅UI线程调用）
    void setConfig(const PipelineConfig& config) { m_config = config; }

    // ========== 算法队列管理 ==========

    void addAlgorithmStep(const AlgorithmStep& step);
    void removeAlgorithmStep(int index);
    void swapAlgorithmStep(int index1,int index2);
    void clearAlgorithmQueue();
    const QVector<AlgorithmStep>& getAlgorithmQueue() const { return m_algorithmQueue; }

    // ========== Pipeline执行 ==========

    // 执行Pipeline处理（可在后台线程调用）
    // 输入：BGR图像 + 配置（步骤通过 ctx.config 读取）
    // 返回：处理结果上下文
    PipelineContext execute(const cv::Mat& inputImage, const PipelineConfig& config);

    // 获取最后一次执行的上下文（线程安全，返回拷贝）
    PipelineContext getLastContext() const {
        QMutexLocker locker(&m_contextMutex);
        return m_lastContext;
    }

    void updateAlgorithmStep(int index, const AlgorithmStep& step);

    void setDisplayMode(DisplayConfig::Mode mode);
    DisplayConfig::Mode getDisplayMode() const { return m_displayMode; }

    // 快速重新渲染上次Pipeline结果
    cv::Mat getLastDisplayWithMode(DisplayConfig::Mode mode) const;

    bool hasLastResult() const;

    // ========== 颜色过滤控制 ==========


    void resetPipeline();

signals:
    void pipelineFinished(const QString& message);
    void algorithmQueueChanged(int count);
    void pipelineReset();

private:
    void initPipeline();

private:
    // 互斥锁：保护 m_lastContext（UI线程读取，execute写入）
    mutable QMutex m_contextMutex;

    // 配置（仅在UI线程读写，不需要锁保护）
    PipelineConfig m_config;

    // 原子标志：后台Pipeline是否正在运行（仅用于reset安全）
    QAtomicInt m_pipelineRunning{0};

    // 延迟重置标志（execute正在执行时，UI线程请求reset暂存于此）
    QAtomicInt m_hasPendingReset{0};

    // Pipeline实例
    Pipeline m_pipeline;

    // 算法队列
    QVector<AlgorithmStep> m_algorithmQueue;

    // 图像处理器
    std::unique_ptr<ImageProcessor> m_processor;

    // 最后执行结果
    PipelineContext m_lastContext;

    DisplayConfig::Mode m_displayMode = DisplayConfig::Mode::MaskGreenWhite;
    float m_overlayAlpha = AppConstants::DEFAULT_OVERLAY_ALPHA;

};
