#pragma once

#include <QJsonObject>
#include <QStringList>
#include <array>
#include <opencv2/core.hpp>
#include "config/shape_filter_types.h"

// ====== 枚举类型（原 pipeline_types.h，合并于此）======

/// Pipeline步骤类型
enum class StepType : int
{
    ColorChannel = 0,
    Enhance,
    GrayFilter,
    ColorFilter,
    AlgorithmQueue,
    ShapeFilter,
    LineDetect,
    ReferenceLineFilter,
    BarcodeRecognition,
    Count   // 步骤总数
};

/// 步骤显示名称（用于UI）
inline const char* stepDisplayName(StepType type)
{
    switch (type) {
        case StepType::ColorChannel:       return "颜色通道";
        case StepType::Enhance:            return "图像增强";
        case StepType::GrayFilter:         return "灰度过滤";
        case StepType::ColorFilter:        return "颜色过滤";
        case StepType::AlgorithmQueue:     return "算法处理";
        case StepType::ShapeFilter:        return "形状筛选";
        case StepType::LineDetect:         return "直线检测";
        case StepType::ReferenceLineFilter: return "参考线匹配";
        case StepType::BarcodeRecognition: return "条码识别";
        default:                           return "未知步骤";
    }
}

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
 * @brief 图像增强配置（亮度、对比度、伽马、锐化）
 *
 * 使用与 UI 滑块一致的整数原始值。
 * 在 Pipeline 执行时由 StepEnhance 内部转换为算法所需的比例值。
 *
 * 注意：此类同时服务于 PipelineConfig（全局流水线）和
 * DetectionItem 各检测类型的独立增强参数。
 */
struct EnhanceConfig
{
    int brightness = 0;    // 亮度 (-100 ~ 100)
    int contrast = 100;    // 对比度 (0 ~ 300)，100 = 100%
    int gamma = 100;       // 伽马值 (10 ~ 300)，100 = 1.0
    int sharpen = 100;     // 锐化 (0 ~ 500)，100 = 1.0

    bool operator==(const EnhanceConfig& o) const {
        return brightness == o.brightness &&
               contrast == o.contrast &&
               gamma == o.gamma &&
               sharpen == o.sharpen;
    }

    QJsonObject toJson() const {
        QJsonObject obj;
        obj["brightness"] = brightness;
        obj["contrast"] = contrast;
        obj["gamma"] = gamma;
        obj["sharpen"] = sharpen;
        return obj;
    }

    void fromJson(const QJsonObject& obj) {
        brightness = obj["brightness"].toInt(0);
        contrast = obj["contrast"].toInt(100);
        gamma = obj["gamma"].toInt(100);
        sharpen = obj["sharpen"].toInt(100);
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
 * @brief 直线检测与参考线匹配配置（统一版本）
 *
 * 合并了原 LineDetectConfig（PipelineConfig 用）和
 * LineDetectionConfig（DetectionItem 用）。
 * 对应 LineTabWidget 的参数。
 */
struct LineDetectConfig
{
    EnhanceConfig enhance;  ///< 图像增强参数（独立实例）

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

    QJsonObject toJson() const;
    void fromJson(const QJsonObject& obj);
};

// 向后兼容别名
using LineDetectionConfig = LineDetectConfig;

/**
 * @brief Blob分析判定配置（区域数量阈值）
 *
 * 对应 JudgeTabWidget 的参数。
 * 仅用于 Pipeline 全局配置中的判定阈值，
 * 与 DetectionItem 中 BlobAnalysisConfig 的 minBlobCount/maxBlobCount 不同。
 */
struct BlobJudgeConfig
{
    int minRegionCount = 0;
    int maxRegionCount = 1000;
    int currentRegionCount = 0;
};

/**
 * @brief 条码识别配置（统一版本）
 *
 * 合并了原 BarcodeConfig（PipelineConfig 用）和
 * BarcodeRecognitionConfig（DetectionItem 用），
 * 消除重复定义。两者字段完全一致。
 */
struct BarcodeConfig
{
    EnhanceConfig enhance;     ///< 图像增强参数（独立实例）

    // 条码识别参数
    bool enableBarcode = false;           // 是否启用条码识别
    QStringList codeTypes = {"auto"};     // 条码类型，"auto"表示自动检测
    int maxNumSymbols = 0;               // 最大识别数量，0=不限制
    bool returnQuality = true;           // 返回质量信息
    double minConfidence = 0.7;          // 最小置信度
    int timeout = 5000;                  // 超时时间（毫秒）

    // 图像预处理参数
    bool enablePreprocessing = true;     // 是否启用预处理
    int preprocessMethod = 0;            // 预处理方法 (0: 直接识别, 1: 二值化, 2: 形态学)
    int binarizationThreshold = 128;     // 二值化阈值
    int morphologySize = 3;              // 形态学核大小

    bool operator==(const BarcodeConfig& o) const {
        return enableBarcode == o.enableBarcode &&
               codeTypes == o.codeTypes &&
               maxNumSymbols == o.maxNumSymbols &&
               returnQuality == o.returnQuality;
    }

    QJsonObject toJson() const;
    void fromJson(const QJsonObject& obj);
};

// 向后兼容别名（原 DetectionItem 体系使用的名字）
using BarcodeRecognitionConfig = BarcodeConfig;

/**
 * @brief Pipeline执行配置结构（全局流水线参数）
 * 
 * ⚠️ 注意：本文件定义的是 Pipeline 执行时的全局配置。
 * 与 detection_config_types.h 中的类型不同——后者是 DetectionItem 
 * （每个检测项独立持有）的专用配置，用于支持一个ROI下挂多个检测项。
 * 
 * 本文件的配置由 PipelineManager 持有，在 processAndDisplay() 时传入 Pipeline 执行。
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
    BlobJudgeConfig  judge;        ///< 判定配置（Blob分析阈值）

    // ========== Pipeline步骤控制 ==========
    static constexpr int STEP_COUNT = static_cast<int>(StepType::Count);

    std::array<bool, STEP_COUNT> stepEnabled = {
        true,   // ColorChannel
        true,   // Enhance
        true,   // GrayFilter
        true,   // ColorFilter
        true,   // AlgorithmQueue
        true,   // ShapeFilter
        true,   // LineDetect
        true,   // ReferenceLineFilter
        true    // BarcodeRecognition
    };

    std::array<int, STEP_COUNT> stepOrder = {0, 1, 2, 3, 4, 5, 6, 7, 8};

    // ========== 扩展功能开关（不在 StepType 枚举中） ==========
    bool enableObjectDetection = false;  ///< 在 Pipeline 末尾启用 YOLO 目标检测

    // ==================== JSON 序列化（定义在 pipeline_config.cpp）====================

    QJsonObject toJson() const;
    void fromJson(const QJsonObject& obj);
};