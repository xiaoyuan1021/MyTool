#pragma once

/**
 * 显示配置结构体
 * 
 * 控制Pipeline处理结果的显示方式
 * 这个结构体是独立的配置类型，不依赖Pipeline核心逻辑
 */
struct DisplayConfig
{
    /**
     * 显示模式枚举
     */
    enum class Mode
    {
        Original,           // 显示原图
        Channel,            // 通道图
        Enhanced,           // 显示增强后的图
        MaskGreenWhite,     // Mask显示为绿白
        MaskOverlay,        // Mask叠加在原图上
        Processed,          // 显示算法处理结果
        LineDetect,         // 显示直线检测结果
        BarcodeOverlay,     // 在原图上叠加条码识别结果
        MaskOnly,           // 直接显示二值Mask（黑白）
        ProcessedOverlay    // 处理结果叠加到原图
    };

    Mode mode = Mode::Original;      // 当前显示模式
    float overlayAlpha = 0.7f;       // 叠加透明度（用于MaskOverlay模式）
};