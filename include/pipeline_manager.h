#pragma once

#include "pipeline.h"
#include "pipeline_steps.h"
#include "image_processor.h"
#include <QObject>
#include <QSlider>
#include <QSpinBox>

/**
 * Pipeline管理器 - 负责Pipeline的创建、配置和执行
 *
 * 职责：
 * 1. 管理Pipeline配置（PipelineConfig）
 * 2. 管理Pipeline实例
 * 3. 管理算法队列
 * 4. 提供统一的执行接口
 */
class PipelineManager : public QObject
{
    Q_OBJECT

public:
    explicit PipelineManager(QObject* parent = nullptr);

    // ========== 配置管理 ==========

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

    // 获取当前配置（只读）
    const PipelineConfig& getConfig() const { return m_config; }

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

    // 执行Pipeline处理
    // 输入：BGR图像
    // 输出：处理结果上下文
    const PipelineContext& execute(const cv::Mat& inputImage);

    // 获取最后一次执行的上下文
    const PipelineContext& getLastContext() const { return m_lastContext; }

    void addFilterCondition(const FilterCondition& condition);
    void setFilterMode(FilterMode mode);
    void clearShapeFilter();
    void enableShapeFilter(bool enable);
    void setFeatureRange(ShapeFeature feature, double minValue, double maxValue);

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
    // 配置
    PipelineConfig m_config;

    // Pipeline实例
    Pipeline m_pipeline;

    // 算法队列
    QVector<AlgorithmStep> m_algorithmQueue;

    // 图像处理器
    std::unique_ptr<imageprocessor> m_processor;

    // 最后执行结果
    PipelineContext m_lastContext;

    DisplayConfig::Mode m_displayMode = DisplayConfig::Mode::MaskGreenWhite;
    float m_overlayAlpha = 0.3f;
};
