#pragma once

#include <QString>
#include <QVariantMap>
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
    QVariantMap parameters;     // 检测参数
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
            default:
                break;
        }
    }

    /**
     * @brief 转换为JSON对象
     */
    QJsonObject toJson() const {
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

    /**
     * @brief 从JSON对象加载
     */
    void fromJson(const QJsonObject& obj) {
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
};