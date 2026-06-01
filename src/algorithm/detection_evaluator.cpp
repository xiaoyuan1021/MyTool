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
        Logger::instance()->debug(QString("[评估] 检测项 '%1' 未启用，跳过").arg(detItem.itemName));
        return DetectionItemResult(detItem.itemId, detItem.itemName, detectionTypeToString(detItem.type));
    }

    Logger::instance()->debug(QString("[评估] 评估检测项: name=%1, type=%2")
        .arg(detItem.itemName).arg(detectionTypeToString(detItem.type)));

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

    // 评估每个检测项
    for (const DetectionItem& detItem : roiConfig.detectionItems) {
        DetectionItemResult itemResult = evaluateItem(detItem, ctx, roiImage, tabMgr);
        roiResult.addItemResult(itemResult);
    }

    Logger::instance()->debug(QString("[评估] ROI '%1' 结果: %2")
        .arg(roiConfig.roiName).arg(roiResult.toSummaryString()));

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

    Logger::instance()->debug(QString("[评估] Blob判定: currentCount=%1, min=%2, max=%3")
        .arg(currentCount).arg(minCount).arg(maxCount));

    if (currentCount < minCount || currentCount > maxCount) {
        result.passed = false;
        result.failReason = QString("Blob数量%1, 要求[%2,%3]")
            .arg(currentCount).arg(minCount).arg(maxCount);
        Logger::instance()->warning(QString("[评估] Blob判定: NG! %1").arg(result.failReason));
    } else {
        Logger::instance()->debug("[评估] Blob判定: OK");
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

    Logger::instance()->debug(QString("[评估] 条码判定: barcodeResults数=%1, minConfidence=%2")
        .arg(ctx.barcodeResults.size()).arg(barcodeConfig.minConfidence));

    if (ctx.barcodeResults.isEmpty()) {
        result.passed = false;
        result.failReason = "未检测到条码";
        Logger::instance()->warning("[评估] 条码判定: NG! 未检测到条码");
    } else {
        Logger::instance()->debug(QString("[评估] 条码结果: 共%1个").arg(ctx.barcodeResults.size()));
        for (const BarcodeResult& br : ctx.barcodeResults) {
            Logger::instance()->debug(QString("  -> type=%1, data=%2, quality=%3")
                .arg(br.type).arg(br.data).arg(br.quality));
        }
        Logger::instance()->debug("[评估] 条码判定: OK");
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

    Logger::instance()->debug(QString("[评估] 直线判定: matchedLineCount=%1, totalLineCount=%2")
        .arg(ctx.matchedLineCount).arg(ctx.totalLineCount));

    if (ctx.matchedLineCount == 0 && ctx.totalLineCount == 0) {
        result.passed = false;
        result.failReason = "未检测到直线";
        Logger::instance()->warning("[评估] 直线判定: NG! 未检测到直线");
    } else {
        Logger::instance()->debug("[评估] 直线判定: OK");
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
    if (objTab) {
        if (objTab->isModelLoaded()) {
            std::vector<DetectionResult> detResults = objTab->runDetection(roiImage);
            detectedCount = static_cast<int>(detResults.size());
            Logger::instance()->debug(QString("[评估] 目标检测执行完成: 检测到%1个目标").arg(detectedCount));
        } else {
            Logger::instance()->warning("[评估] 目标检测模型未加载，跳过数量检查");
        }
    } else {
        Logger::instance()->warning("[评估] 目标检测Tab未找到");
    }

    int expectedCount = objConfig.expectedCount;

    Logger::instance()->debug(QString("[评估] 目标检测判定: detectedCount=%1, expectedCount=%2")
        .arg(detectedCount).arg(expectedCount));

    if (expectedCount > 0 && detectedCount != expectedCount) {
        result.passed = false;
        result.failReason = QString("检测到%1个目标, 期望%2个")
            .arg(detectedCount).arg(expectedCount);
        Logger::instance()->warning(QString("[评估] 目标检测判定: NG! %1").arg(result.failReason));
    } else {
        Logger::instance()->debug("[评估] 目标检测判定: OK");
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

    Logger::instance()->debug(QString("[评估] OCR判定: recognizedText='%1', expectedText='%2'")
        .arg(recognizedText).arg(expectedText));

    if (expectedText.isEmpty()) {
        if (recognizedText.isEmpty()) {
            result.passed = false;
            result.failReason = "未识别到文字";
            Logger::instance()->warning("[评估] OCR判定: NG! 未识别到文字");
        } else {
            Logger::instance()->debug("[评估] OCR判定: OK（未设置期望文本）");
        }
    } else {
        bool matched = false;
        if (ocrConfig.matchExact) {
            matched = (recognizedText == expectedText);
        } else {
            matched = recognizedText.contains(expectedText);
        }

        if (!matched) {
            result.passed = false;
            result.failReason = QString("OCR文本'%1'不匹配'%2'")
                .arg(recognizedText.isEmpty() ? "(空)" : recognizedText)
                .arg(expectedText);
            Logger::instance()->warning(QString("[评估] OCR判定: NG! %1").arg(result.failReason));
        } else {
            Logger::instance()->debug("[评估] OCR判定: OK");
        }
    }

    return result;
}
