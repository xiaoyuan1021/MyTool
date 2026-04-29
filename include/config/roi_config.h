#pragma once

#include <QString>
#include <QRectF>
#include <QList>
#include <QJsonObject>
#include <QJsonArray>
#include "detection_type.h"
#include "tab_config.h"
#include "detection_item.h"
#include "pipeline_config.h"

/**
 * @brief ROI配置结构
 */
struct RoiConfig {
    QString roiId;              // ROI唯一ID
    QString roiName;            // ROI名称
    QRectF roiRect;             // ROI区域（图像坐标）
    bool isActive;              // 是否激活
    bool isSelected;            // 是否选中
    QString color;              // 显示颜色
    QList<DetectionItem> detectionItems; // 检测项列表
    PipelineConfig pipelineConfig; // 该ROI关联的Pipeline配置（亮度、对比度等）

    RoiConfig() 
        : isActive(true), isSelected(false), color("#FF6B6B") {}

    RoiConfig(const QString& name, const QRectF& rect)
        : roiName(name), roiRect(rect), isActive(true), isSelected(false), color("#FF6B6B") {
        // 生成唯一ID
        roiId = generateUniqueId("roi");
    }

    /**
     * @brief 添加检测项
     */
    void addDetectionItem(const DetectionItem& item) {
        detectionItems.append(item);
    }

    /**
     * @brief 删除检测项
     */
    bool removeDetectionItem(const QString& itemId) {
        for (int i = 0; i < detectionItems.size(); ++i) {
            if (detectionItems[i].itemId == itemId) {
                detectionItems.removeAt(i);
                return true;
            }
        }
        return false;
    }

    /**
     * @brief 获取检测项
     */
    DetectionItem* getDetectionItem(const QString& itemId) {
        for (auto& item : detectionItems) {
            if (item.itemId == itemId) {
                return &item;
            }
        }
        return nullptr;
    }

    /**
     * @brief 转换为JSON对象
     */
    QJsonObject toJson() const {
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

    /**
     * @brief 从JSON对象加载
     */
    void fromJson(const QJsonObject& obj) {
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
};