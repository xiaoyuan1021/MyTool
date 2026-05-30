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
    Filter,             // 统一过滤（灰度/RGB/HSV）
    AlgorithmQueue,
    ShapeFilter,
    LineDetector,       // 直线检测（含参考线匹配）
    BarcodeRecognition,
    ImageFilter,        // 滤波去噪
    OcrRecognition,     // OCR文字识别
    Count               // 步骤总数
};

/// 步骤显示名称（用于UI）
inline const char* stepDisplayName(StepType type)
{
    switch (type) {
        case StepType::ColorChannel:       return "颜色通道";
        case StepType::Enhance:            return "图像增强";
        case StepType::Filter:             return "颜色过滤";
        case StepType::AlgorithmQueue:     return "算法处理";
        case StepType::ShapeFilter:        return "形状筛选";
        case StepType::LineDetector:       return "直线检测";
        case StepType::BarcodeRecognition: return "条码识别";
        case StepType::ImageFilter:        return "滤波去噪";
        case StepType::OcrRecognition:     return "文字识别";
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

/// 滤波去噪类型
enum class FilterDenoiseType
{
    Gaussian = 0,   // 高斯滤波
    Median,         // 中值滤波
    Bilateral,      // 双边滤波
    Morphology      // 形态学滤波
};

/// 形态学操作类型
enum class MorphologyOpType
{
    Open = 0,   // 开运算（去小颗粒）
    Close,      // 闭运算（填小孔）
    Erode,      // 腐蚀
    Dilate      // 膨胀
};

/**
 * @brief 滤波去噪配置
 *
 * 支持高斯滤波、中值滤波、双边滤波、形态学滤波四种类型
 */
struct ImageFilterConfig
{
    FilterDenoiseType filterType = FilterDenoiseType::Gaussian;

    // 高斯滤波参数
    int gaussianKernelSize = 3;     // 核大小（奇数）
    double gaussianSigmaX = 1.0;    // X方向sigma
    double gaussianSigmaY = 0.0;    // Y方向sigma（0=与sigmaX相同）

    // 中值滤波参数
    int medianKernelSize = 3;       // 核大小（奇数）

    // 双边滤波参数
    int bilateralD = 9;             // 像素邻域直径
    double bilateralSigmaColor = 75.0;  // 颜色空间sigma
    double bilateralSigmaSpace = 75.0;  // 坐标空间sigma

    // 形态学滤波参数
    MorphologyOpType morphologyOp = MorphologyOpType::Open;
    int morphologyKernelSize = 3;   // 核大小
    int morphologyIterations = 1;   // 迭代次数

    bool operator==(const ImageFilterConfig& o) const {
        return filterType == o.filterType &&
               gaussianKernelSize == o.gaussianKernelSize &&
               gaussianSigmaX == o.gaussianSigmaX &&
               gaussianSigmaY == o.gaussianSigmaY &&
               medianKernelSize == o.medianKernelSize &&
               bilateralD == o.bilateralD &&
               bilateralSigmaColor == o.bilateralSigmaColor &&
               bilateralSigmaSpace == o.bilateralSigmaSpace &&
               morphologyOp == o.morphologyOp &&
               morphologyKernelSize == o.morphologyKernelSize &&
               morphologyIterations == o.morphologyIterations;
    }

    QJsonObject toJson() const {
        QJsonObject obj;
        obj["filterType"] = static_cast<int>(filterType);
        obj["gaussianKernelSize"] = gaussianKernelSize;
        obj["gaussianSigmaX"] = gaussianSigmaX;
        obj["gaussianSigmaY"] = gaussianSigmaY;
        obj["medianKernelSize"] = medianKernelSize;
        obj["bilateralD"] = bilateralD;
        obj["bilateralSigmaColor"] = bilateralSigmaColor;
        obj["bilateralSigmaSpace"] = bilateralSigmaSpace;
        obj["morphologyOp"] = static_cast<int>(morphologyOp);
        obj["morphologyKernelSize"] = morphologyKernelSize;
        obj["morphologyIterations"] = morphologyIterations;
        return obj;
    }

    void fromJson(const QJsonObject& obj) {
        filterType = static_cast<FilterDenoiseType>(obj["filterType"].toInt(0));
        gaussianKernelSize = obj["gaussianKernelSize"].toInt(3);
        gaussianSigmaX = obj["gaussianSigmaX"].toDouble(1.0);
        gaussianSigmaY = obj["gaussianSigmaY"].toDouble(0.0);
        medianKernelSize = obj["medianKernelSize"].toInt(3);
        bilateralD = obj["bilateralD"].toInt(9);
        bilateralSigmaColor = obj["bilateralSigmaColor"].toDouble(75.0);
        bilateralSigmaSpace = obj["bilateralSigmaSpace"].toDouble(75.0);
        morphologyOp = static_cast<MorphologyOpType>(obj["morphologyOp"].toInt(0));
        morphologyKernelSize = obj["morphologyKernelSize"].toInt(3);
        morphologyIterations = obj["morphologyIterations"].toInt(1);
    }
};

/**
 * @brief OCR文字识别配置
 *
 * 基于Tesseract OCR引擎，支持中英文识别
 */
struct OcrConfig
{
    QString language = "chi_sim+eng";    // 识别语言（chi_sim=中文，eng=英文）
    int pageMode = 0;                     // 页面分割模式（0=自动，1=单行，2=多行）
    int dpi = 0;                          // 图像分辨率（0=自动估计）
    double confidenceThreshold = 0.3;     // 最小置信度阈值 (0~1)
    QString whitelist = "";               // 字符白名单（空=不过滤）

    bool operator==(const OcrConfig& o) const {
        return language == o.language &&
               pageMode == o.pageMode &&
               dpi == o.dpi &&
               confidenceThreshold == o.confidenceThreshold &&
               whitelist == o.whitelist;
    }

    QJsonObject toJson() const {
        QJsonObject obj;
        obj["language"] = language;
        obj["pageMode"] = pageMode;
        obj["dpi"] = dpi;
        obj["confidenceThreshold"] = confidenceThreshold;
        obj["whitelist"] = whitelist;
        return obj;
    }

    void fromJson(const QJsonObject& obj) {
        language = obj["language"].toString("chi_sim+eng");
        pageMode = obj["pageMode"].toInt(0);
        dpi = obj["dpi"].toInt(0);
        confidenceThreshold = obj["confidenceThreshold"].toDouble(0.3);
        whitelist = obj["whitelist"].toString("");
    }
};

/**
 * @brief 目标检测配置参数
 */
struct ObjectDetectionConfig {
    // 模型参数
    QString modelPath = "";                 // 模型文件路径
    QString configPath = "";                // 配置文件路径（可选）
    
    // 检测参数
    float confidenceThreshold = 0.5f;       // 置信度阈值 (0.0 ~ 1.0)
    float nmsThreshold = 0.4f;              // 非极大值抑制阈值 (0.0 ~ 1.0)
    int inputWidth = 640;                    // 输入宽度
    int inputHeight = 640;                   // 输入高度
    
    // 显示参数
    bool showLabels = true;                  // 是否显示标签
    bool showConfidence = true;              // 是否显示置信度
    bool showBoundingBox = true;             // 是否显示边框
    int lineWidth = 2;                       // 边框线宽
    
    // 判定参数
    int expectedCount = 0;                   // 期望检测数量（0=不检查数量）
    
    // JSON 序列化
    QJsonObject toJson() const {
        QJsonObject obj;
        obj["modelPath"] = modelPath;
        obj["configPath"] = configPath;
        obj["confidenceThreshold"] = static_cast<double>(confidenceThreshold);
        obj["nmsThreshold"] = static_cast<double>(nmsThreshold);
        obj["inputWidth"] = inputWidth;
        obj["inputHeight"] = inputHeight;
        obj["showLabels"] = showLabels;
        obj["showConfidence"] = showConfidence;
        obj["showBoundingBox"] = showBoundingBox;
        obj["lineWidth"] = lineWidth;
        obj["expectedCount"] = expectedCount;
        return obj;
    }
    
    void fromJson(const QJsonObject& obj) {
        modelPath = obj["modelPath"].toString();
        configPath = obj["configPath"].toString();
        confidenceThreshold = static_cast<float>(obj["confidenceThreshold"].toDouble(0.5));
        nmsThreshold = static_cast<float>(obj["nmsThreshold"].toDouble(0.4));
        inputWidth = obj["inputWidth"].toInt(640);
        inputHeight = obj["inputHeight"].toInt(640);
        showLabels = obj["showLabels"].toBool(true);
        showConfidence = obj["showConfidence"].toBool(true);
        showBoundingBox = obj["showBoundingBox"].toBool(true);
        lineWidth = obj["lineWidth"].toInt(2);
        expectedCount = obj["expectedCount"].toInt(0);
    }
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

    // 向后兼容：旧字段转换
    [[deprecated]] bool enableGrayFilter = true;
    [[deprecated]] bool enableColorFilter = false;
    [[deprecated]] ::ColorFilterMode colorFilterMode = ::ColorFilterMode::None;
    [[deprecated]] ImageFilterMode currentFilterMode = ImageFilterMode::None;
    [[deprecated]] bool enableAreaFilter = false;
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
    ImageFilterConfig imageFilter; ///< 滤波去噪
    OcrConfig        ocr;          ///< OCR文字识别
    ObjectDetectionConfig objectDetection; ///< 目标检测配置

    // ========== Pipeline步骤控制 ==========
    static constexpr int STEP_COUNT = static_cast<int>(StepType::Count);

    std::array<bool, STEP_COUNT> stepEnabled = {
        false,  // ColorChannel
        false,  // Enhance
        false,  // Filter
        false,  // AlgorithmQueue
        false,  // ShapeFilter
        false,  // LineDetector
        false,  // BarcodeRecognition
        false,  // ImageFilter
        false   // OcrRecognition
    };

    std::array<int, STEP_COUNT> stepOrder = {0, 1, 2, 3, 4, 5, 6, 7, 8};

    // ========== 扩展功能开关（不在 StepType 枚举中） ==========
    bool enableObjectDetection = false;  ///< 在 Pipeline 末尾启用 YOLO 目标检测

    /**
     * @brief 重置所有配置为出厂默认值
     *
     * 切换图片、ROI 或无配置时调用，防止旧配置污染新任务。
     * 不修改 stepOrder/stepEnabled（由 rebuildPipeline 管理）。
     */
    void resetToDefaults();

    // ==================== JSON 序列化（定义在 pipeline_config.cpp）====================

    QJsonObject toJson() const;
    void fromJson(const QJsonObject& obj);
};