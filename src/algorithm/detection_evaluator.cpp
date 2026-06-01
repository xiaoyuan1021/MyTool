#include "algorithm/detection_evaluator.h"
#include "config/detection_config_types.h"
#include "widgets/object_detection_tab_widget.h"
#include "widgets/tab_manager.h"
#include "logger.h"

DetectionItemResult DetectionEvaluator::evaluateItem(
    const DetectionItem& detItem,
    const PipelineContext& ctx,
    const cv::Mat& roiImage,
    void* tabMgr)
{
    if (!detItem.enabled) {
        return DetectionItemResult(detItem.itemId, detItem.itemName, detectionTypeToString(detItem.type));
    }

    DetectionItemResult result;
    switch (detItem.type) {
        case DetectionType::Blob:
            result = evaluateBlob(detItem, ctx);
            break;
        case DetectionType::Barcode:
            result = evaluateBarcode(detItem, ctx);
            break;
        case DetectionType::Line:
            result = evaluateLine(detItem, ctx);
            break;
        case DetectionType::ObjectDetection:
            result = evaluateObjectDetection(detItem, roiImage, tabMgr);
            break;
        case DetectionType::Ocr:
            result = evaluateOcr(detItem, ctx);
            break;
        default:
            result = DetectionItemResult(detItem.itemId, detItem.itemName, detectionTypeToString(detItem.type));
            Logger::instance()->warning(QString("[评估] 未知检测类型: %1").arg(detectionTypeToString(detItem.type)));
            break;
    }

    result.itemId = detItem.itemId;
    result.itemName = detItem.itemName;
    result.detectionType = detectionTypeToString(detItem.type);
    return result;
}

RoiDetectionResult DetectionEvaluator::evaluateRoi(
    const RoiConfig& roiConfig,
    const PipelineContext& ctx,
    const cv::Mat& roiImage,
    void* tabMgr)
{
    RoiDetectionResult roiResult(roiConfig.roiId, roiConfig.roiName, QString());

    // 填充Pipeline检测结果到ROI结果
    roiResult.regionCount = ctx.regionCount;
    roiResult.regionFeatures = ctx.regionFeatures;
    roiResult.barcodeResults = ctx.barcodeResults;
    roiResult.ocrText = ctx.ocrText;
    roiResult.ocrRegions = ctx.ocrRegions;
    roiResult.matchedLineCount = ctx.matchedLineCount;
    roiResult.totalLineCount = ctx.totalLineCount;

    // ★ Pipeline执行失败时，直接标记ROI为FAIL，不再评估检测项
    if (!ctx.pass) {
        DetectionItemResult pipelineItem("pipeline", "Pipeline执行", "Pipeline");
        pipelineItem.passed = false;
        pipelineItem.failReason = ctx.reason.isEmpty() ? "Pipeline执行失败" : ctx.reason;
        roiResult.addItemResult(pipelineItem);
        Logger::instance()->warning(QString("[评估] ROI '%1' Pipeline执行失败: %2")
            .arg(roiConfig.roiName, pipelineItem.failReason));
        return roiResult;
    }

    // 评估每个检测项
    for (const DetectionItem& detItem : roiConfig.detectionItems) {
        DetectionItemResult itemResult = evaluateItem(detItem, ctx, roiImage, tabMgr);
        roiResult.addItemResult(itemResult);
    }

    // 只在失败时打印日志
    if (!roiResult.passed) {
        Logger::instance()->warning(QString("[评估] %1").arg(roiResult.toSummaryString()));
    }

    return roiResult;
}

// ==================== 私有评估方法 ====================

DetectionItemResult DetectionEvaluator::evaluateBlob(
    const DetectionItem& detItem,
    const PipelineContext& ctx)
{
    DetectionItemResult result;
    BlobAnalysisConfig blobConfig;
    blobConfig.fromJson(detItem.config);

    int currentCount = ctx.regionCount;
    int minCount = blobConfig.minBlobCount;
    int maxCount = blobConfig.maxBlobCount;

    if (currentCount < minCount || currentCount > maxCount) {
        result.passed = false;
        result.failReason = QString("Blob数量%1, 要求[%2,%3]")
            .arg(currentCount).arg(minCount).arg(maxCount);
    }

    return result;
}

DetectionItemResult DetectionEvaluator::evaluateBarcode(
    const DetectionItem& detItem,
    const PipelineContext& ctx)
{
    DetectionItemResult result;
    BarcodeRecognitionConfig barcodeConfig;
    barcodeConfig.fromJson(detItem.config);

    if (ctx.barcodeResults.isEmpty()) {
        result.passed = false;
        result.failReason = "未检测到条码";
    }

    return result;
}

DetectionItemResult DetectionEvaluator::evaluateLine(
    const DetectionItem& detItem,
    const PipelineContext& ctx)
{
    DetectionItemResult result;
    LineDetectionConfig lineConfig;
    lineConfig.fromJson(detItem.config);

    if (ctx.matchedLineCount == 0 && ctx.totalLineCount == 0) {
        result.passed = false;
        result.failReason = "未检测到直线";
    }

    return result;
}

DetectionItemResult DetectionEvaluator::evaluateObjectDetection(
    const DetectionItem& detItem,
    const cv::Mat& roiImage,
    void* tabMgr)
{
    DetectionItemResult result;
    ObjectDetectionConfig objConfig;
    objConfig.fromJson(detItem.config);

    int detectedCount = 0;
    auto* mgr = static_cast<TabManager*>(tabMgr);
    auto* objTab = mgr ? mgr->getTabAs<ObjectDetectionTabWidget>("目标检测") : nullptr;
    if (objTab && objTab->isModelLoaded()) {
        std::vector<DetectionResult> detResults = objTab->runDetection(roiImage);
        detectedCount = static_cast<int>(detResults.size());
    }

    int expectedCount = objConfig.expectedCount;
    if (expectedCount > 0 && detectedCount != expectedCount) {
        result.passed = false;
        result.failReason = QString("检测到%1个目标, 期望%2个")
            .arg(detectedCount).arg(expectedCount);
    }

    return result;
}

DetectionItemResult DetectionEvaluator::evaluateOcr(
    const DetectionItem& detItem,
    const PipelineContext& ctx)
{
    DetectionItemResult result;
    OcrDetectionConfig ocrConfig;
    ocrConfig.fromJson(detItem.config);

    QString recognizedText = ctx.ocrText.trimmed();
    QString expectedText = ocrConfig.expectedText.trimmed();

    if (expectedText.isEmpty()) {
        if (recognizedText.isEmpty()) {
            result.passed = false;
            result.failReason = "未识别到文字";
        }
    } else {
        bool matched = ocrConfig.matchExact
            ? (recognizedText == expectedText)
            : recognizedText.contains(expectedText);

        if (!matched) {
            result.passed = false;
            result.failReason = QString("OCR文本'%1'不匹配'%2'")
                .arg(recognizedText.isEmpty() ? "(空)" : recognizedText)
                .arg(expectedText);
        }
    }

    return result;
}
