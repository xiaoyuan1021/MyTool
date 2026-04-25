#include "config/pipeline_config_mapper.h"
#include <algorithm>

void PipelineConfigMapper::syncConfigFromUI(PipelineConfig& config,
                                            QSlider* brightness, QSlider* contrast, QSlider* gamma,
                                            QSlider* sharpen, QSlider* grayLow, QSlider* grayHigh)
{
    // 添加空指针检查，防止访问已删除的对象
    if (!brightness || !contrast || !gamma || !sharpen || !grayLow || !grayHigh)
        return;

    config.brightness = brightness->value();
    config.contrast   = contrast->value() / 100.0;
    config.gamma      = gamma->value() / 100.0;
    config.sharpen    = sharpen->value() / 100.0;
    config.grayLow    = grayLow->value();
    config.grayHigh   = grayHigh->value();

    // 防呆：避免 low > high
    if (config.grayLow > config.grayHigh)
        std::swap(config.grayLow, config.grayHigh);
}

void PipelineConfigMapper::resetEnhancement(PipelineConfig& config)
{
    config.brightness = 0;
    config.contrast = 1.0;
    config.gamma = 1.0;
    config.sharpen = 1.0;
    config.enableGrayFilter = false;
}