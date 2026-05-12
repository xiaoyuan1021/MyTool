#include "config/pipeline_config_mapper.h"
#include <algorithm>

void PipelineConfigMapper::syncConfigFromUI(PipelineConfig& config,
                                            QSlider* brightness, QSlider* contrast, QSlider* gamma,
                                            QSlider* sharpen, QSlider* grayLow, QSlider* grayHigh)
{
    // 添加空指针检查，防止访问已删除的对象
    if (!brightness || !contrast || !gamma || !sharpen || !grayLow || !grayHigh)
        return;

    config.enhance.brightness = brightness->value();
    config.enhance.contrast   = contrast->value() / 100.0;
    config.enhance.gamma      = gamma->value() / 100.0;
    config.enhance.sharpen    = sharpen->value() / 100.0;
    config.colorFilter.grayLow    = grayLow->value();
    config.colorFilter.grayHigh   = grayHigh->value();

    // 防呆：避免 low > high
    if (config.colorFilter.grayLow > config.colorFilter.grayHigh)
        std::swap(config.colorFilter.grayLow, config.colorFilter.grayHigh);
}

void PipelineConfigMapper::resetEnhancement(PipelineConfig& config)
{
    config.enhance.brightness = 0;
    config.enhance.contrast = 1.0;
    config.enhance.gamma = 1.0;
    config.enhance.sharpen = 1.0;
    config.colorFilter.enableGrayFilter = false;
}
