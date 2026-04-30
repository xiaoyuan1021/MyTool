#pragma once

#include <QString>
#include <QVariantMap>
#include <QJsonObject>
#include <QJsonArray>
#include "detection_type.h"
#include "detection_config_types.h"
#include "vision_inspection_config.h"

/**
 * @brief 检测项配置结构
 *
 * 使用 QJsonObject 存储具体检测类型的配置，新增检测类型时
 * 无需修改此结构体，只需在对应类型中实现 toJson/fromJson 即可。
 * 
 * 对于目标检测等需要视频源的类型，使用visionConfig统一管理。
 */
struct DetectionItem {
    QString itemId;             // 检测项唯一ID
    QString itemName;           // 检测项名称
    DetectionType type;         // 检测类型
    QVariantMap parameters;     // 检测参数
    bool enabled;               // 是否启用
    QString description;        // 描述信息
    QJsonObject config;         // 检测类型的专属配置（通用JSON存储）
    
    // 视觉检测配置（用于需要视频源的检测类型，如目标检测）
    VisionInspectionConfig visionConfig;
    bool useVisionConfig = false;  // 是否使用视觉检测配置

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
                // 目标检测使用视觉检测配置
                useVisionConfig = true;
                visionConfig.detectionMethod = DetectionType::ObjectDetection;
                visionConfig.detectionConfig = ObjectDetectionConfig().toJson();
                config = ObjectDetectionConfig().toJson();
                break;
            default:
                break;
        }
    }
    
    /**
     * @brief 创建目标检测项（使用视觉检测配置）
     */
    static DetectionItem createObjectDetectionItem(const QString& name = "目标检测") {
        DetectionItem item(name, DetectionType::ObjectDetection);
        item.useVisionConfig = true;
        item.visionConfig = VisionInspectionConfig();
        item.visionConfig.detectionMethod = DetectionType::ObjectDetection;
        item.visionConfig.detectionConfig = ObjectDetectionConfig().toJson();
        return item;
    }
    
    /**
     * @brief 创建视频源检测项（使用视觉检测配置）
     */
    static DetectionItem createVideoSourceItem(const QString& name = "视频源") {
        DetectionItem item(name, DetectionType::VideoSource);
        item.useVisionConfig = true;
        item.visionConfig = VisionInspectionConfig();
        item.visionConfig.videoSourceType = VisionInspectionConfig::VideoSourceType::Camera;
        item.visionConfig.detectionMethod = DetectionType::Blob;
        return item;
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
        
        // 视觉检测配置
        if (useVisionConfig) {
            obj["useVisionConfig"] = true;
            obj["visionConfig"] = visionConfig.toJson();
        }
        
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
        
        // 加载视觉检测配置
        if (obj.contains("useVisionConfig")) {
            useVisionConfig = obj["useVisionConfig"].toBool(false);
            if (useVisionConfig && obj.contains("visionConfig")) {
                visionConfig.fromJson(obj["visionConfig"].toObject());
            }
        }
        
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
                default:
                    break;
            }
        }
    }
};