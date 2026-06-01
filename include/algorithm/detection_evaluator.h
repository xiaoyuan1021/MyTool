#pragma once

#include "config/roi_config.h"
#include "config/detection_config_types.h"
#include "core/pipeline.h"
#include "data/roi_detection_result.h"

/**
 * 检测项评估器
 *
 * 封装了根据 PipelineContext 和 DetectionItem 配置判定 pass/fail 的逻辑。
 * 从 AutoDetectionController 中提取，便于复用和单元测试。
 *
 * 使用方式：
 *   RoiDetectionResult result = DetectionEvaluator::evaluateRoi(
 *       roiConfig, ctx, roiImage, tabMgrPtr);
 */
class DetectionEvaluator
{
public:
    /**
     * 评估单个检测项
     * @param detItem 检测项配置
     * @param ctx Pipeline执行结果（单ROI）
     * @param roiImage ROI裁剪图像（目标检测需要）
     * @param tabMgr TabManager指针（目标检测需要，可为nullptr）
     * @return 检测项评估结果
     */
    static DetectionItemResult evaluateItem(
        const DetectionItem& detItem,
        const PipelineContext& ctx,
        const cv::Mat& roiImage,
        void* tabMgr = nullptr);

    /**
     * 评估一个ROI的所有检测项，构建完整的RoiDetectionResult
     * @param roiConfig ROI配置（含检测项列表）
     * @param ctx Pipeline执行结果（单ROI）
     * @param roiImage ROI裁剪图像
     * @param tabMgr TabManager指针（可为nullptr）
     * @return ROI级别的检测结果
     */
    static RoiDetectionResult evaluateRoi(
        const RoiConfig& roiConfig,
        const PipelineContext& ctx,
        const cv::Mat& roiImage,
        void* tabMgr = nullptr);

private:
    static DetectionItemResult evaluateBlob(
        const DetectionItem& detItem,
        const PipelineContext& ctx);

    static DetectionItemResult evaluateBarcode(
        const DetectionItem& detItem,
        const PipelineContext& ctx);

    static DetectionItemResult evaluateLine(
        const DetectionItem& detItem,
        const PipelineContext& ctx);

    static DetectionItemResult evaluateObjectDetection(
        const DetectionItem& detItem,
        const cv::Mat& roiImage,
        void* tabMgr);

    static DetectionItemResult evaluateOcr(
        const DetectionItem& detItem,
        const PipelineContext& ctx);
};
