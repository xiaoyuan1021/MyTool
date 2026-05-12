#pragma once

#include <QJsonObject>
#include <opencv2/core.hpp>
#include "core/pipeline_types.h"
#include "config/shape_filter_types.h"

/**
 * @brief 图像增强配置（亮度、对比度、伽马、锐化）
 *
 * 对应 EnhanceTabWidget 的参数。
 */
struct EnhanceConfig
{
    int brightness = 0;
    double contrast = 1.0;
    double gamma = 1.0;
    double sharpen = 0.0;

    bool operator==(const EnhanceConfig& o) const {
        return brightness == o.brightness &&
               qFuzzyCompare(contrast, o.contrast) &&
               qFuzzyCompare(gamma, o.gamma) &&
               qFuzzyCompare(sharpen, o.sharpen);
    }
};

/**
 * @brief 颜色/通道/灰度过滤配置
 *
 * 对应 FilterTabWidget 和颜色过滤相关参数。
 */
struct ColorFilterConfig
{
    ChannelMode channel = ChannelMode::RGB;
    ImageFilterMode currentFilterMode = ImageFilterMode::None;

    bool enableColorFilter = false;
    ::ColorFilterMode colorFilterMode = ::ColorFilterMode::None;

    // 灰度过滤（选中范围）
    int grayLow = 0;
    int grayHigh = 255;
    bool enableGrayFilter = true;
    bool enableAreaFilter = false;

    // RGB 过滤范围
    int rLow = 0, rHigh = 255;
    int gLow = 0, gHigh = 255;
    int bLow = 0, bHigh = 255;

    // HSV 过滤范围
    int hLow = 0, hHigh = 179;
    int sLow = 0, sHigh = 255;
    int vLow = 0, vHigh = 255;
};

/**
 * @brief 直线检测与参考线匹配配置
 *
 * 对应 LineTabWidget 的参数。
 */
struct LineDetectConfig
{
    int algorithm = 0;      // 0=HoughP, 1=LSD, 2=EDline
    double rho = 1.0;
    double theta = CV_PI / 180.0;
    int threshold = 50;
    double minLength = 30.0;
    double maxGap = 10.0;
    bool enabled = false;

    // 参考线匹配参数
    bool enableReferenceLineMatch = false;
    cv::Point2f referenceLineStart;
    cv::Point2f referenceLineEnd;
    bool referenceLineValid = false;
    double angleThreshold = 15.0;      // 角度容差（度）
    double distanceThreshold = 50.0;   // 距离容差（像素）
    int searchRegionWidth = 100;       // 搜索区域宽度（像素）
};

/**
 * @brief 判定配置（Blob分析阈值）
 *
 * 对应 JudgeTabWidget 的参数。
 */
struct JudgeConfig
{
    int minRegionCount = 0;
    int maxRegionCount = 1000;
    int currentRegionCount = 0;
};

/**
 * @brief Pipeline配置结构
 *
 * 按功能分组为子结构体，降低单一"胖结构体"的维护风险。
 * 每个子结构体独立管理自己的职责领域。
 *
 * 历史：从 pipeline.h 中提取，用于降低依赖耦合。
 */
struct PipelineConfig
{
    // 使用独立定义的枚举类型（向后兼容的别名）
    using Channel = ChannelMode;
    using ColorFilterMode = ::ColorFilterMode;
    using FilterMode = ::ImageFilterMode;

    // ========== 按功能分组的子配置 ==========
    EnhanceConfig    enhance;      ///< 图像增强（亮度/对比度/伽马/锐化）
    ColorFilterConfig colorFilter; ///< 颜色/通道/灰度过滤
    LineDetectConfig lineDetect;   ///< 直线检测与参考线匹配
    ShapeFilterConfig shapeFilter; ///< 形状筛选
    BarcodeConfig    barcode;      ///< 条码识别
    JudgeConfig      judge;        ///< 判定配置（Blob分析阈值）

    // ==================== JSON 序列化 ====================

    /**
     * @brief 将 PipelineConfig 序列化为 JSON
     *        各子结构体独立序列化，集中管理所有字段
     */
    QJsonObject toJson() const {
        QJsonObject obj;

        // 增强参数
        QJsonObject enhanceObj;
        enhanceObj["brightness"] = enhance.brightness;
        enhanceObj["contrast"] = enhance.contrast;
        enhanceObj["gamma"] = enhance.gamma;
        enhanceObj["sharpen"] = enhance.sharpen;
        obj["enhance"] = enhanceObj;

        // 通道/过滤参数
        QJsonObject colorObj;
        colorObj["channel"] = static_cast<int>(colorFilter.channel);
        colorObj["currentFilterMode"] = static_cast<int>(colorFilter.currentFilterMode);
        colorObj["enableColorFilter"] = colorFilter.enableColorFilter;
        colorObj["colorFilterMode"] = static_cast<int>(colorFilter.colorFilterMode);
        colorObj["grayLow"] = colorFilter.grayLow;
        colorObj["grayHigh"] = colorFilter.grayHigh;
        colorObj["enableGrayFilter"] = colorFilter.enableGrayFilter;
        colorObj["enableAreaFilter"] = colorFilter.enableAreaFilter;
        colorObj["rLow"] = colorFilter.rLow;
        colorObj["rHigh"] = colorFilter.rHigh;
        colorObj["gLow"] = colorFilter.gLow;
        colorObj["gHigh"] = colorFilter.gHigh;
        colorObj["bLow"] = colorFilter.bLow;
        colorObj["bHigh"] = colorFilter.bHigh;
        colorObj["hLow"] = colorFilter.hLow;
        colorObj["hHigh"] = colorFilter.hHigh;
        colorObj["sLow"] = colorFilter.sLow;
        colorObj["sHigh"] = colorFilter.sHigh;
        colorObj["vLow"] = colorFilter.vLow;
        colorObj["vHigh"] = colorFilter.vHigh;
        obj["colorFilter"] = colorObj;

        // 直线检测参数
        QJsonObject lineObj;
        lineObj["algorithm"] = lineDetect.algorithm;
        lineObj["threshold"] = lineDetect.threshold;
        lineObj["minLength"] = lineDetect.minLength;
        lineObj["maxGap"] = lineDetect.maxGap;
        lineObj["enabled"] = lineDetect.enabled;
        obj["lineDetect"] = lineObj;

        // 条码参数
        QJsonObject barcodeObj;
        barcodeObj["enableBarcode"] = barcode.enableBarcode;
        barcodeObj["codeTypes"] = barcode.codeTypes.join(",");
        barcodeObj["maxNumSymbols"] = barcode.maxNumSymbols;
        barcodeObj["returnQuality"] = barcode.returnQuality;
        obj["barcode"] = barcodeObj;

        return obj;
    }

    /**
     * @brief 从 JSON 反序列化 PipelineConfig
     *        忽略 JSON 中不存在的字段，保持当前值
     */
    void fromJson(const QJsonObject& obj) {
        if (obj.isEmpty()) return;

        // 增强参数
        if (obj.contains("enhance")) {
            QJsonObject enhanceObj = obj["enhance"].toObject();
            enhance.brightness = enhanceObj["brightness"].toInt(0);
            enhance.contrast   = enhanceObj["contrast"].toDouble(1.0);
            enhance.gamma      = enhanceObj["gamma"].toDouble(1.0);
            enhance.sharpen    = enhanceObj["sharpen"].toDouble(0.0);
        } else {
            // 向后兼容：旧格式直接平铺在顶层
            enhance.brightness = obj["brightness"].toInt(0);
            enhance.contrast   = obj["contrast"].toDouble(1.0);
            enhance.gamma      = obj["gamma"].toDouble(1.0);
            enhance.sharpen    = obj["sharpen"].toDouble(0.0);
        }

        // 通道/过滤参数
        if (obj.contains("colorFilter")) {
            QJsonObject colorObj = obj["colorFilter"].toObject();
            colorFilter.channel          = static_cast<ChannelMode>(colorObj["channel"].toInt(0));
            colorFilter.currentFilterMode = static_cast<ImageFilterMode>(colorObj["currentFilterMode"].toInt(0));
            colorFilter.enableColorFilter = colorObj["enableColorFilter"].toBool(false);
            colorFilter.colorFilterMode  = static_cast<::ColorFilterMode>(colorObj["colorFilterMode"].toInt(0));
            colorFilter.grayLow          = colorObj["grayLow"].toInt(0);
            colorFilter.grayHigh         = colorObj["grayHigh"].toInt(255);
            colorFilter.enableGrayFilter = colorObj["enableGrayFilter"].toBool(true);
            colorFilter.enableAreaFilter = colorObj["enableAreaFilter"].toBool(false);
            colorFilter.rLow = colorObj["rLow"].toInt(0);
            colorFilter.rHigh = colorObj["rHigh"].toInt(255);
            colorFilter.gLow = colorObj["gLow"].toInt(0);
            colorFilter.gHigh = colorObj["gHigh"].toInt(255);
            colorFilter.bLow = colorObj["bLow"].toInt(0);
            colorFilter.bHigh = colorObj["bHigh"].toInt(255);
            colorFilter.hLow = colorObj["hLow"].toInt(0);
            colorFilter.hHigh = colorObj["hHigh"].toInt(179);
            colorFilter.sLow = colorObj["sLow"].toInt(0);
            colorFilter.sHigh = colorObj["sHigh"].toInt(255);
            colorFilter.vLow = colorObj["vLow"].toInt(0);
            colorFilter.vHigh = colorObj["vHigh"].toInt(255);
        } else {
            // 向后兼容：旧格式平铺在顶层
            colorFilter.channel = static_cast<ChannelMode>(obj["channel"].toInt(0));
        }

        // 直线检测参数
        if (obj.contains("lineDetect")) {
            QJsonObject lineObj = obj["lineDetect"].toObject();
            lineDetect.algorithm   = lineObj["algorithm"].toInt(0);
            lineDetect.threshold   = lineObj["threshold"].toInt(50);
            lineDetect.minLength   = lineObj["minLength"].toDouble(30.0);
            lineDetect.maxGap      = lineObj["maxGap"].toDouble(10.0);
            lineDetect.enabled     = lineObj["enabled"].toBool(false);
        }

        // 条码参数
        QJsonObject barcodeObj = obj["barcode"].toObject();
        if (!barcodeObj.isEmpty()) {
            barcode.enableBarcode = barcodeObj["enableBarcode"].toBool(false);
            QString codeTypesStr  = barcodeObj["codeTypes"].toString();
            if (!codeTypesStr.isEmpty()) {
                barcode.codeTypes = codeTypesStr.split(",");
            }
            barcode.maxNumSymbols = barcodeObj["maxNumSymbols"].toInt(0);
            barcode.returnQuality = barcodeObj["returnQuality"].toBool(true);
        }
    }
};