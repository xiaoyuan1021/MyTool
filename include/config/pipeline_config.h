#pragma once

/**
 * @brief Pipeline 配置聚合头文件
 *
 * 本文件聚合所有独立的配置头文件，保持向后兼容。
 * 新代码应直接 include 需要的具体配置头文件（如 enhance_config.h），
 * 而非包含本文件，以减少编译依赖传播。
 */

#include <array>
#include "algorithm_step.h"
#include "config/shape_filter_types.h"

// 独立配置头文件
#include "config/enhance_config.h"
#include "config/color_filter_config.h"
#include "config/image_filter_config.h"
#include "config/ocr_config.h"
#include "config/object_detection_config.h"
#include "config/line_detect_config.h"
#include "config/blob_judge_config.h"
#include "config/barcode_config.h"

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
    bool enableObjectDetection = false;  ///< Pipeline步骤：目标检测步骤是否启用（StepConfigWidget控制）
    bool objectDetectionApplyEnabled = false;  ///< Apply按钮：用户是否点击应用启用检测（ObjectDetectionTab控制）

    // ========== 算法队列（每个ROI独立保存） ==========
    QVector<AlgorithmStep> algorithmQueue;  ///< 形态学等算法步骤队列

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
