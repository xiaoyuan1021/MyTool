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

    /**
     * 检查是否应该显示绿白Mask
     */
    bool shouldShowGreenWhite() const
    {
        return mode == Mode::MaskGreenWhite;
    }

    /**
     * 检查是否应该叠加Mask
     */
    bool shouldOverlay() const
    {
        return mode == Mode::MaskOverlay;
    }

    /**
     * 检查是否显示处理结果
     */
    bool shouldShowProcessed() const
    {
        return mode == Mode::Processed;
    }

    /**
     * 检查是否显示直线检测结果
     */
    bool shouldShowLineDetect() const
    {
        return mode == Mode::LineDetect;
    }

    /**
     * 检查是否显示条码叠加
     */
    bool shouldShowBarcodeOverlay() const
    {
        return mode == Mode::BarcodeOverlay;
    }

    /**
     * 检查是否显示原始Mask
     */
    bool shouldShowMaskOnly() const
    {
        return mode == Mode::MaskOnly;
    }

    /**
     * 检查是否显示处理结果叠加
     */
    bool shouldShowProcessedOverlay() const
    {
        return mode == Mode::ProcessedOverlay;
    }

    /**
     * 设置显示模式
     */
    void setMode(Mode newMode)
    {
        mode = newMode;
    }

    /**
     * 设置叠加透明度
     */
    void setOverlayAlpha(float alpha)
    {
        overlayAlpha = (alpha >= 0.0f && alpha <= 1.0f) ? alpha : 0.7f;
    }

    /**
     * 重置为默认配置
     */
    void reset()
    {
        mode = Mode::Original;
        overlayAlpha = 0.7f;
    }
};