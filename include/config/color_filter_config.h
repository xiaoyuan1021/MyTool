#pragma once

#include <QJsonObject>

/// 颜色通道模式
enum class ChannelMode
{
    Gray,   // 灰度图
    RGB,    // RGB彩色图
    HSV,    // HSV色彩空间
    B,      // 蓝色通道
    G,      // 绿色通道
    R       // 红色通道
};

/// 颜色过滤模式
enum class ColorFilterMode
{
    None,   // 无颜色过滤
    RGB,    // RGB颜色过滤
    HSV     // HSV颜色过滤
};

/// 图像过滤模式（用于选择使用哪种图像过滤方式）
/// 注意：与shape_filter_types.h中的FilterMode（And/Or逻辑模式）不同
enum class ImageFilterMode
{
    None,   // 无过滤，只显示增强后的图像
    Gray,   // 灰度过滤模式
    RGB,    // RGB颜色过滤模式
    HSV     // HSV颜色过滤模式
};

/**
 * @brief 颜色/通道/灰度过滤配置
 *
 * 对应 FilterTabWidget 和颜色过滤相关参数。
 * 统一使用 mode 字段控制过滤类型，无需单独的 enable 标志。
 */
struct ColorFilterConfig
{
    ChannelMode channel = ChannelMode::RGB;
    ImageFilterMode mode = ImageFilterMode::None;  // 统一过滤模式开关

    // 灰度过滤（选中范围）
    int grayLow = 0;
    int grayHigh = 255;

    // RGB 过滤范围
    int rLow = 0, rHigh = 255;
    int gLow = 0, gHigh = 255;
    int bLow = 0, bHigh = 255;

    // HSV 过滤范围
    int hLow = 0, hHigh = 179;
    int sLow = 0, sHigh = 255;
    int vLow = 0, vHigh = 255;
};
