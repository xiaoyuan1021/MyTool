#pragma once

#include <QSlider>
#include "core/pipeline.h"

/**
 * @brief UI 配置映射器
 * 
 * 负责 UI 控件与 PipelineConfig 之间的数据同步
 * 解耦核心层 PipelineConfig 对 Qt UI 控件的直接依赖
 */
class PipelineConfigMapper
{
public:
    /**
     * @brief 从 UI 滑块控件同步配置到 PipelineConfig
     * @param config 目标配置对象
     * @param brightness 亮度滑块
     * @param contrast 对比度滑块
     * @param gamma 伽马滑块
     * @param sharpen 锐化滑块
     * @param grayLow 灰度下限滑块
     * @param grayHigh 灰度上限滑块
     */
    static void syncConfigFromUI(PipelineConfig& config,
                                 QSlider* brightness, QSlider* contrast, QSlider* gamma,
                                 QSlider* sharpen, QSlider* grayLow, QSlider* grayHigh);

    /**
     * @brief 重置增强参数到默认值
     * @param config 目标配置对象
     */
    static void resetEnhancement(PipelineConfig& config);
};