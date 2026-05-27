#include "config/detection_item.h"

QJsonObject DetectionItem::toJson() const {
    QJsonObject obj;
    obj["itemId"] = itemId;
    obj["itemName"] = itemName;
    obj["type"] = detectionTypeToString(type);
    obj["enabled"] = enabled;
    obj["description"] = description;
    
    obj["config"] = config;
    
    return obj;
}

void DetectionItem::fromJson(const QJsonObject& obj) {
    itemId = obj["itemId"].toString();
    itemName = obj["itemName"].toString();
    type = stringToDetectionType(obj["type"].toString());
    enabled = obj["enabled"].toBool(true);
    description = obj["description"].toString();
    
    config = obj["config"].toObject();
}