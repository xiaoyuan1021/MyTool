#pragma once

#include <QString>
#include <QJsonObject>
#include <QJsonArray>
#include "detection_type.h"
#include "detection_config_types.h"

/**
 * @brief 检测项配置结构
 *
 * 使用 QJsonObject 存储具体检测类型的配置，新增检测类型时
 * 无需修改此结构体，只需在对应类型中实现 toJson/fromJson 即可。
 */
struct DetectionItem {
    QString itemId;             // 检测项唯一ID
    QString itemName;           // 检测项名称
    DetectionType type;         // 检测类型
    bool enabled;               // 是否启用
    QString description;        // 描述信息
    QJsonObject config;         // 检测类型的专属配置（通用JSON存储）

    DetectionItem() 
        : type(DetectionType::Blob), enabled(true) {}

    DetectionItem(const QString& name, DetectionType t)
        : itemName(name), type(t), enabled(true) {
        itemId = generateUniqueId("det");
        
        // 根据类型初始化对应的默认配置
        switch (type) {
            case DetectionType::Blob:
                config = BlobAnalysisConfig().toJson();
                break;
            case DetectionType::Line:
                config = LineDetectionConfig().toJson();
                break;
            case DetectionType::Barcode:
                config = BarcodeRecognitionConfig().toJson();
                break;
            case DetectionType::VideoSource:
                config = VideoSourceConfig().toJson();
                break;
            case DetectionType::ObjectDetection:
                config = ObjectDetectionConfig().toJson();
                break;
            case DetectionType::VideoDetection:
                config = VideoDetectionConfig().toJson();
                break;
            case DetectionType::Ocr:
                config = OcrDetectionConfig().toJson();
                break;
            default:
                break;
        }
    }

    // JSON 序列化（定义在 detection_item.cpp）
    QJsonObject toJson() const;
    void fromJson(const QJsonObject& obj);
};