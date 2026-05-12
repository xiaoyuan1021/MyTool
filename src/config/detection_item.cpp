#include "config/detection_item.h"

QJsonObject DetectionItem::toJson() const {
    QJsonObject obj;
    obj["itemId"] = itemId;
    obj["itemName"] = itemName;
    obj["type"] = detectionTypeToString(type);
    obj["enabled"] = enabled;
    obj["description"] = description;
    
    QJsonObject params;
    for (auto it = parameters.begin(); it != parameters.end(); ++it) {
        params[it.key()] = QJsonValue::fromVariant(it.value());
    }
    obj["parameters"] = params;
    obj["config"] = config;
    
    return obj;
}

void DetectionItem::fromJson(const QJsonObject& obj) {
    itemId = obj["itemId"].toString();
    itemName = obj["itemName"].toString();
    type = stringToDetectionType(obj["type"].toString());
    enabled = obj["enabled"].toBool(true);
    description = obj["description"].toString();
    
    QJsonObject params = obj["parameters"].toObject();
    for (auto it = params.begin(); it != params.end(); ++it) {
        parameters[it.key()] = it.value().toVariant();
    }
    
    config = obj["config"].toObject();
    
    // 兼容旧版本：如果config为空，尝试从旧字段加载
    if (config.isEmpty()) {
        switch (type) {
            case DetectionType::Blob:
                if (obj.contains("blobConfig")) {
                    config = obj["blobConfig"].toObject();
                } else {
                    config = BlobAnalysisConfig().toJson();
                }
                break;
            case DetectionType::Line:
                if (obj.contains("lineConfig")) {
                    config = obj["lineConfig"].toObject();
                } else {
                    config = LineDetectionConfig().toJson();
                }
                break;
            case DetectionType::Barcode:
                if (obj.contains("barcodeConfig")) {
                    config = obj["barcodeConfig"].toObject();
                } else {
                    config = BarcodeRecognitionConfig().toJson();
                }
                break;
            case DetectionType::VideoSource:
                if (obj.contains("videoConfig")) {
                    config = obj["videoConfig"].toObject();
                } else {
                    config = VideoSourceConfig().toJson();
                }
                break;
            case DetectionType::ObjectDetection:
                if (obj.contains("objectDetectionConfig")) {
                    config = obj["objectDetectionConfig"].toObject();
                } else {
                    config = ObjectDetectionConfig().toJson();
                }
                break;
            case DetectionType::VideoDetection:
                if (obj.contains("videoDetectionConfig")) {
                    config = obj["videoDetectionConfig"].toObject();
                } else {
                    config = VideoDetectionConfig().toJson();
                }
                break;
            default:
                break;
        }
    }
}