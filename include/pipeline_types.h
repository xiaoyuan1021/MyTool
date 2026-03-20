#pragma once

/**
 * Pipeline相关的枚举类型定义
 * 
 * 将这些枚举从PipelineConfig中分离出来，好处：
 * 1. 这些类型可以被独立引用，不需要依赖整个PipelineConfig
 * 2. 减少头文件的相互依赖
 * 3. 便于扩展新的枚举类型
 */

/**
 * 颜色通道模式
 */
enum class ChannelMode
{
    Gray,   // 灰度图
    RGB,    // RGB彩色图
    HSV,    // HSV色彩空间
    B,      // 蓝色通道
    G,      // 绿色通道
    R       // 红色通道
};

/**
 * 颜色过滤模式
 */
enum class ColorFilterMode
{
    None,   // 无颜色过滤
    RGB,    // RGB颜色过滤
    HSV     // HSV颜色过滤
};

/**
 * 图像过滤模式（用于选择使用哪种图像过滤方式）
 * 注意：与shape_filter_types.h中的FilterMode（And/Or逻辑模式）不同
 */
enum class ImageFilterMode
{
    None,   // 无过滤，只显示增强后的图像
    Gray,   // 灰度过滤模式
    RGB,    // RGB颜色过滤模式
    HSV     // HSV颜色过滤模式
};
