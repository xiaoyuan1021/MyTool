#include "config/roi_config.h"

QJsonObject RoiConfig::toJson() const {
    QJsonObject obj;
    obj["roiId"] = roiId;
    obj["roiName"] = roiName;
    obj["isActive"] = isActive;
    obj["isSelected"] = isSelected;
    obj["color"] = color;
    
    QJsonObject rect;
    rect["x"] = roiRect.x();
    rect["y"] = roiRect.y();
    rect["width"] = roiRect.width();
    rect["height"] = roiRect.height();
    obj["roiRect"] = rect;
    
    QJsonArray detections;
    for (const auto& item : detectionItems) {
        detections.append(item.toJson());
    }
    obj["detectionItems"] = detections;
    
    obj["pipelineConfig"] = pipelineConfig.toJson();
    
    return obj;
}

void RoiConfig::fromJson(const QJsonObject& obj) {
    roiId = obj["roiId"].toString();
    roiName = obj["roiName"].toString();
    isActive = obj["isActive"].toBool(true);
    isSelected = obj["isSelected"].toBool(false);
    color = obj["color"].toString("#FF6B6B");
    
    QJsonObject rect = obj["roiRect"].toObject();
    roiRect = QRectF(
        rect["x"].toDouble(),
        rect["y"].toDouble(),
        rect["width"].toDouble(),
        rect["height"].toDouble()
    );
    
    detectionItems.clear();
    QJsonArray detections = obj["detectionItems"].toArray();
    for (const auto& val : detections) {
        DetectionItem item;
        item.fromJson(val.toObject());
        detectionItems.append(item);
    }
    
    pipelineConfig.fromJson(obj["pipelineConfig"].toObject());
}