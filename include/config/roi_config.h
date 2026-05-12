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

    // JSON 序列化（定义在 roi_config.cpp）
    QJsonObject toJson() const;
    void fromJson(const QJsonObject& obj);
};