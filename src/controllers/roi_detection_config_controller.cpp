#include "controllers/roi_detection_config_controller.h"
#include "logger.h"

RoiDetectionConfigController::RoiDetectionConfigController(
    RoiManager& roiManager,
    QObject* parent)
    : QObject(parent)
    , m_roiManager(roiManager)
{
}

void RoiDetectionConfigController::updateBlobDetectionConfig(int minCount, int maxCount)
{
    RoiConfig* roi = m_roiManager.getRoiConfig(m_currentRoiId);
    if (!roi) return;
    for (auto& detItem : roi->detectionItems) {
        if (detItem.type == DetectionType::Blob && detItem.enabled) {
            BlobAnalysisConfig blobConfig;
            blobConfig.fromJson(detItem.config);
            blobConfig.minBlobCount = minCount;
            blobConfig.maxBlobCount = maxCount;
            detItem.config = blobConfig.toJson();
            Logger::instance()->info(QString("[RoiDetectionConfig] 判定阈值已更新: min=%1, max=%2").arg(minCount).arg(maxCount));
            break;
        }
    }
}

void RoiDetectionConfigController::updateOcrDetectionConfig(const QString& expectedText, bool matchExact)
{
    RoiConfig* roi = m_roiManager.getRoiConfig(m_currentRoiId);
    if (!roi) return;
    for (auto& detItem : roi->detectionItems) {
        if (detItem.type == DetectionType::Ocr && detItem.enabled) {
            OcrDetectionConfig ocrConfig;
            ocrConfig.fromJson(detItem.config);
            ocrConfig.expectedText = expectedText;
            ocrConfig.matchExact = matchExact;
            detItem.config = ocrConfig.toJson();
            Logger::instance()->info(QString("[RoiDetectionConfig] OCR判定参数已更新: expectedText='%1', matchExact=%2").arg(expectedText).arg(matchExact));
            break;
        }
    }
}

void RoiDetectionConfigController::updateObjectDetectionConfig(const ObjectDetectionConfig& objConfig)
{
    RoiConfig* roi = m_roiManager.getRoiConfig(m_currentRoiId);
    if (!roi) return;
    for (auto& detItem : roi->detectionItems) {
        if (detItem.type == DetectionType::ObjectDetection && detItem.enabled) {
            detItem.config = objConfig.toJson();
            Logger::instance()->info(QString("[RoiDetectionConfig] 目标检测配置已更新: model=%1").arg(objConfig.modelPath));
            break;
        }
    }
}

void RoiDetectionConfigController::updateBarcodeDetectionConfig(const BarcodeConfig& barcodeConfig)
{
    RoiConfig* roi = m_roiManager.getRoiConfig(m_currentRoiId);
    if (!roi) return;
    for (auto& detItem : roi->detectionItems) {
        if (detItem.type == DetectionType::Barcode && detItem.enabled) {
            detItem.config = barcodeConfig.toJson();
            Logger::instance()->info(QString("[RoiDetectionConfig] 条码检测配置已更新"));
            break;
        }
    }
}

void RoiDetectionConfigController::updateLineDetectionConfig(const LineDetectConfig& lineConfig)
{
    RoiConfig* roi = m_roiManager.getRoiConfig(m_currentRoiId);
    if (!roi) return;
    for (auto& detItem : roi->detectionItems) {
        if (detItem.type == DetectionType::Line && detItem.enabled) {
            detItem.config = lineConfig.toJson();
            Logger::instance()->info(QString("[RoiDetectionConfig] 直线检测配置已更新"));
            break;
        }
    }
}
