#pragma once

#include <QString>
#include <QRectF>
#include <QList>
#include <QVariantMap>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonDocument>
#include <QDateTime>
#include <QRandomGenerator>
#include "detection_config_types.h"

/**
 * @brief 检测类型枚举
 */
enum class DetectionType {
    Barcode,        // 条码检测
    Shape,          // 形状检测
    Color,          // 颜色检测
    Texture,        // 纹理检测
    Template,       // 模板匹配
    Line,           // 直线检测
    Circle,         // 圆形检测
    Blob,           // Blob分析
    VideoSource     // 视频源
};

/**
 * @brief 将检测类型转换为字符串
 */
inline QString detectionTypeToString(DetectionType type) {
    switch (type) {
        case DetectionType::Barcode: return "条码检测";
        case DetectionType::Shape: return "形状检测";
        case DetectionType::Color: return "颜色检测";
        case DetectionType::Texture: return "纹理检测";
        case DetectionType::Template: return "模板匹配";
        case DetectionType::Line: return "直线检测";
        case DetectionType::Circle: return "圆形检测";
        case DetectionType::Blob: return "Blob分析";
        case DetectionType::VideoSource: return "视频源";
        default: return "未知类型";
    }
}

/**
 * @brief 将字符串转换为检测类型
 */
inline DetectionType stringToDetectionType(const QString& str) {
    if (str == "条码检测") return DetectionType::Barcode;
    if (str == "形状检测") return DetectionType::Shape;
    if (str == "颜色检测") return DetectionType::Color;
    if (str == "纹理检测") return DetectionType::Texture;
    if (str == "模板匹配") return DetectionType::Template;
    if (str == "直线检测") return DetectionType::Line;
    if (str == "圆形检测") return DetectionType::Circle;
    if (str == "Blob分析") return DetectionType::Blob;
    if (str == "视频源") return DetectionType::VideoSource;
    return DetectionType::Blob; // 默认值
}

/**
 * @brief Tab配置结构
 * 定义每种检测类型需要显示的Tab列表
 */
struct TabConfig {
    QStringList tabNames;      // Tab名称列表
    QList<int> tabIndices;     // Tab索引列表（动态维护）
    
    TabConfig() {}
    
    TabConfig(const QStringList& names) : tabNames(names) {}
    
    /**
     * @brief 获取Blob分析的Tab配置
     */
    static TabConfig getBlobConfig() {
        return TabConfig({
            "图像",
            "增强",
            "过滤",
            "补正",
            "处理",
            "提取",
            "判定"
        });
    }
    
    /**
     * @brief 获取直线检测的Tab配置
     */
    static TabConfig getLineConfig() {
        return TabConfig({
            "图像",
            "增强",
            "直线"
        });
    }
    
    /**
     * @brief 获取条码识别的Tab配置
     */
    static TabConfig getBarcodeConfig() {
        return TabConfig({
            "图像",
            "增强",
            "条码"
        });
    }
    
    /**
     * @brief 获取视频源的Tab配置
     */
    static TabConfig getVideoConfig() {
        return TabConfig({
            "图像",
            "视频",
            "增强",
            "过滤",
            "处理",
            "提取",
            "判定"
        });
    }
    
    /**
     * @brief 根据检测类型获取Tab配置
     */
    static TabConfig getConfigForType(DetectionType type) {
        switch (type) {
            case DetectionType::Blob:
                return getBlobConfig();
            case DetectionType::Line:
                return getLineConfig();
            case DetectionType::Barcode:
                return getBarcodeConfig();
            case DetectionType::VideoSource:
                return getVideoConfig();
            default:
                return TabConfig();
        }
    }
};

/**
 * @brief 生成唯一ID
 * @param prefix 前缀（如 "roi" 或 "det"）
 * @return 唯一ID字符串
 */
inline QString generateUniqueId(const QString& prefix) {
    return QString("%1_%2_%3")
        .arg(prefix)
        .arg(QDateTime::currentMSecsSinceEpoch())
        .arg(QRandomGenerator::global()->bounded(10000));
}

/**
 * @brief 检测项配置结构
 */
struct DetectionItem {
    QString itemId;             // 检测项唯一ID
    QString itemName;           // 检测项名称
    DetectionType type;         // 检测类型
    QVariantMap parameters;     // 检测参数
    bool enabled;               // 是否启用
    QString description;        // 描述信息

    // 特定类型的配置
    BlobAnalysisConfig blobConfig;           // Blob分析配置
    LineDetectionConfig lineConfig;          // 直线检测配置
    BarcodeRecognitionConfig barcodeConfig;  // 条码识别配置
    VideoSourceConfig videoConfig;           // 视频源配置

    DetectionItem() 
        : type(DetectionType::Blob), enabled(true) {}

    DetectionItem(const QString& name, DetectionType t)
        : itemName(name), type(t), enabled(true) {
        // 生成唯一ID
        itemId = generateUniqueId("det");
        
        // 根据类型初始化对应的配置
        switch (type) {
            case DetectionType::Blob:
                blobConfig = BlobAnalysisConfig();
                break;
            case DetectionType::Line:
                lineConfig = LineDetectionConfig();
                break;
            case DetectionType::Barcode:
                barcodeConfig = BarcodeRecognitionConfig();
                break;
            case DetectionType::VideoSource:
                videoConfig = VideoSourceConfig();
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
        
        // 根据类型保存特定配置
        switch (type) {
            case DetectionType::Blob:
                obj["blobConfig"] = blobConfig.toJson();
                break;
            case DetectionType::Line:
                obj["lineConfig"] = lineConfig.toJson();
                break;
            case DetectionType::Barcode:
                obj["barcodeConfig"] = barcodeConfig.toJson();
                break;
            case DetectionType::VideoSource:
                obj["videoConfig"] = videoConfig.toJson();
                break;
            default:
                break;
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
        
        // 根据类型加载特定配置
        switch (type) {
            case DetectionType::Blob:
                if (obj.contains("blobConfig")) {
                    blobConfig.fromJson(obj["blobConfig"].toObject());
                }
                break;
            case DetectionType::Line:
                if (obj.contains("lineConfig")) {
                    lineConfig.fromJson(obj["lineConfig"].toObject());
                }
                break;
            case DetectionType::Barcode:
                if (obj.contains("barcodeConfig")) {
                    barcodeConfig.fromJson(obj["barcodeConfig"].toObject());
                }
                break;
            case DetectionType::VideoSource:
                if (obj.contains("videoConfig")) {
                    videoConfig.fromJson(obj["videoConfig"].toObject());
                }
                break;
            default:
                break;
        }
    }
};

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
    }
};

/**
 * @brief 多ROI配置管理类
 */
class MultiRoiConfig {
public:
    MultiRoiConfig() = default;

    /**
     * @brief 添加ROI
     */
    void addRoi(const RoiConfig& roi) {
        m_rois.append(roi);
    }

    /**
     * @brief 删除ROI
     */
    bool removeRoi(const QString& roiId) {
        for (int i = 0; i < m_rois.size(); ++i) {
            if (m_rois[i].roiId == roiId) {
                m_rois.removeAt(i);
                return true;
            }
        }
        return false;
    }

    /**
     * @brief 获取ROI
     */
    RoiConfig* getRoi(const QString& roiId) {
        for (auto& roi : m_rois) {
            if (roi.roiId == roiId) {
                return &roi;
            }
        }
        return nullptr;
    }

    /**
     * @brief 获取所有ROI
     */
    QList<RoiConfig>& getRois() { return m_rois; }
    const QList<RoiConfig>& getRois() const { return m_rois; }

    /**
     * @brief 清空所有ROI
     */
    void clear() { m_rois.clear(); }

    /**
     * @brief 获取ROI数量
     */
    int size() const { return m_rois.size(); }

    /**
     * @brief 检查是否为空
     */
    bool isEmpty() const { return m_rois.isEmpty(); }

    /**
     * @brief 转换为JSON文档
     */
    QJsonDocument toJsonDocument() const {
        QJsonArray rois;
        for (const auto& roi : m_rois) {
            rois.append(roi.toJson());
        }
        
        QJsonObject root;
        root["version"] = "1.0";
        root["roiCount"] = m_rois.size();
        root["rois"] = rois;
        
        return QJsonDocument(root);
    }

    /**
     * @brief 从JSON文档加载
     */
    void fromJsonDocument(const QJsonDocument& doc) {
        m_rois.clear();
        
        QJsonObject root = doc.object();
        QJsonArray rois = root["rois"].toArray();
        
        for (const auto& val : rois) {
            RoiConfig roi;
            roi.fromJson(val.toObject());
            m_rois.append(roi);
        }
    }

private:
    QList<RoiConfig> m_rois;
};