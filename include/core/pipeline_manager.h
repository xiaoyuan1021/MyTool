#pragma once

#include "pipeline.h"
#include "pipeline_steps.h"
#include "image_processor.h"
#include <QObject>
#include <QSlider>
#include <QSpinBox>
#include <QString>
#include <QMutex>
#include <QMutexLocker>
#include <QAtomicInt>

/**
 * Pipeline管理器 - 负责Pipeline的创建、配置和执行
 *
 * 线程模型：
 * - 配置管理方法（setConfig/getConfigSnapshot等）仅在UI线程调用，无需加锁
 * - execute() 在后台线程调用，使用m_configMutex保护Pipeline执行期间的状态
 * - UI线程和后台线程之间通过值拷贝传递数据，避免共享可变状态
 */
class PipelineManager : public QObject
{
    Q_OBJECT

public:
    explicit PipelineManager(QObject* parent = nullptr);

    // ========== 配置管理（仅UI线程调用）==========

    // 从UI控件同步参数到配置
    void syncFromUI(QSlider* brightness, QSlider* contrast, QSlider* gamma,
                    QSlider* sharpen, QSlider* grayLow, QSlider* grayHigh);

    // 重置增强参数
    void resetEnhancement();

    // 设置灰度过滤开关
    void setGrayFilterEnabled(bool enabled);

    void setAreaFilterEnabled(bool enabled);

    // 设置面积筛选范围
    void setAreaRange(double minArea, double maxArea);

    // 获取当前配置快照（永不阻塞UI线程）
    // m_config仅在UI线程读写，直接返回拷贝
    PipelineConfig getConfigSnapshot() const {
        return m_config;
    }

    // 设置配置（永不阻塞UI线程）
    // 如果Pipeline正在执行，将配置存入pending队列
    void setConfig(const PipelineConfig& config) {
        m_config = config;
        m_lastConfigSnapshot = config;
        // 如果后台线程正在执行Pipeline，标记pending让worker完成后应用
        if (m_pipelineRunning.loadAcquire()) {
            m_pendingConfig = config;
            m_hasPendingConfig.storeRelease(1);
        }
    }

    // ========== 算法队列管理 ==========

    // 添加算法步骤
    void addAlgorithmStep(const AlgorithmStep& step);

    // 移除指定位置的算法步骤
    void removeAlgorithmStep(int index);

    void swapAlgorithmStep(int index1,int index2);

    // 清空算法队列
    void clearAlgorithmQueue();

    // 获取算法队列（只读）
    const QVector<AlgorithmStep>& getAlgorithmQueue() const { return m_algorithmQueue; }

    // ========== Pipeline执行 ==========

    // 执行Pipeline处理（在后台线程调用）
    // 输入：BGR图像 + 配置快照（值拷贝）
    // 输出：处理结果上下文
    // 内部会加锁，确保执行期间Pipeline状态不被并发修改
    PipelineContext execute(const cv::Mat& inputImage, const PipelineConfig& config);

    // 获取最后一次执行的上下文
    const PipelineContext& getLastContext() const { return m_lastContext; }

    void addFilterCondition(const FilterCondition& condition);
    void setFilterMode(FilterMode mode);
    void clearShapeFilter();
    void enableShapeFilter(bool enable);
    void setFeatureRange(ShapeFeature feature, double minValue, double maxValue);
    void setShapeFilterConfig(const ShapeFilterConfig& config);

    void setChannelMode(PipelineConfig::Channel channel);
    PipelineConfig::Channel getChannelMode() const;
    void updateAlgorithmStep(int index, const AlgorithmStep& step);

    void setDisplayMode(DisplayConfig::Mode mode);

    void setOverlayAlpha(float alpha);

    // ========== 颜色过滤控制 ==========

    // 设置颜色过滤开关
    void setColorFilterEnabled(bool enabled);

    // 设置颜色过滤模式
    void setColorFilterMode(PipelineConfig::ColorFilterMode mode);

    // 设置 RGB 过滤范围
    void setRGBRange(int rLow, int rHigh, int gLow, int gHigh, int bLow, int bHigh);

    // 设置 HSV 过滤范围
    void setHSVRange(int hLow, int hHigh, int sLow, int sHigh, int vLow, int vHigh);

    void setCurrentFilterMode(PipelineConfig::FilterMode mode);

    PipelineConfig::FilterMode getCurrentFilterMode() const;

    void resetPipeline();

signals:
    // Pipeline执行完成
    void pipelineFinished(const QString& message);

    // 算法队列变化
    void algorithmQueueChanged(int count);

    // Pipeline重置完成
    void pipelineReset();

private:
    // 初始化Pipeline（创建所有步骤）
    void initPipeline();

    // 重建Pipeline（当步骤变化时）
    void rebuildPipeline();

private:
    // 互斥锁：仅保护Pipeline执行期间的m_pipeline和m_lastContext
    mutable QMutex m_configMutex;

    // 配置（仅在UI线程读写，不需要锁保护）
    PipelineConfig m_config;

    // 缓存快照：供后台execute时使用
    PipelineConfig m_lastConfigSnapshot;

    // 原子标志：后台Pipeline是否正在运行
    QAtomicInt m_pipelineRunning{0};

    // 延迟应用的配置（后台线程正在执行时，UI线程写入的配置暂存于此）
    PipelineConfig m_pendingConfig;
    QAtomicInt m_hasPendingConfig{0};

    // 延迟重置标志
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
    float m_overlayAlpha = 0.3f;

};