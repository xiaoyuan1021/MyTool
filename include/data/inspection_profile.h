#pragma once

#include <QString>
#include <QRectF>
#include <QList>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonDocument>
#include <QDateTime>
#include <QSize>
#include <QFile>
#include <QDir>
#include <opencv2/opencv.hpp>
#include "config/roi_config.h"
#include "algorithm/match_strategy.h"

/**
 * @brief 方案中保存的模板条目
 *
 * 按名称保存模板图像和参数，方便跨图片复用。
 * 存储路径结构：
 *   profiles/<profileName>/templates/<templateName>.png
 *   profiles/<profileName>/templates/<templateName>.json
 */
struct TemplateEntry
{
    QString templateName;        ///< 模板名称（如"螺丝A"、"标签B"）
    QString templateImagePath;   // 模板图片相对路径（相对于方案目录）
    QString templateJsonPath;    // 模板参数相对路径
    cv::Mat templateImage;       // 模板图像（运行时加载，不序列化）
    TemplateParams params;       // 模板匹配参数

    TemplateEntry() : params() {
        params.matchMethod = 5; // TM_CCOEFF_NORMED 默认值
    }

    TemplateEntry(const QString& name)
        : templateName(name), params() {
        params.matchMethod = 5;
    }

    /**
     * @brief 从文件加载模板图像和参数
     * @param baseDir 方案根目录
     */
    bool load(const QString& baseDir) {
        QString imgPath = baseDir + "/templates/" + templateName + ".png";
        QString jsonPath = baseDir + "/templates/" + templateName + ".json";

        // 加载图像
        templateImage = cv::imread(imgPath.toStdString(), cv::IMREAD_COLOR);
        if (templateImage.empty()) {
            return false;
        }

        // 加载参数
        QFile file(jsonPath);
        if (file.open(QIODevice::ReadOnly)) {
            QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
            QJsonObject obj = doc.object();
            params.matchMethod = obj["matchMethod"].toInt(5);

            params.polygonPoints.clear();
            QJsonArray pts = obj["polygonPoints"].toArray();
            for (const auto& pt : pts) {
                QJsonObject ptObj = pt.toObject();
                params.polygonPoints.append(QPointF(
                    ptObj["x"].toDouble(), ptObj["y"].toDouble()));
            }
            file.close();
        }

        templateImagePath = "templates/" + templateName + ".png";
        templateJsonPath = "templates/" + templateName + ".json";
        return true;
    }

    /**
     * @brief 保存模板图像和参数到方案目录
     * @param baseDir 方案根目录
     */
    bool save(const QString& baseDir) {
        QDir dir(baseDir);
        if (!dir.exists("templates")) {
            dir.mkpath("templates");
        }

        QString imgPath = baseDir + "/templates/" + templateName + ".png";
        QString jsonPath = baseDir + "/templates/" + templateName + ".json";

        // 保存图像
        if (!templateImage.empty()) {
            if (!cv::imwrite(imgPath.toStdString(), templateImage)) {
                return false;
            }
        }

        // 保存参数
        QJsonObject obj;
        obj["matchMethod"] = params.matchMethod;

        QJsonArray pts;
        for (const auto& pt : params.polygonPoints) {
            QJsonObject ptObj;
            ptObj["x"] = pt.x();
            ptObj["y"] = pt.y();
            pts.append(ptObj);
        }
        obj["polygonPoints"] = pts;

        QFile file(jsonPath);
        if (file.open(QIODevice::WriteOnly)) {
            file.write(QJsonDocument(obj).toJson());
            file.close();
        }

        templateImagePath = "templates/" + templateName + ".png";
        templateJsonPath = "templates/" + templateName + ".json";
        return true;
    }

    QJsonObject toJson() const {
        QJsonObject obj;
        obj["templateName"] = templateName;
        obj["templateImagePath"] = templateImagePath;
        obj["templateJsonPath"] = templateJsonPath;
        return obj;
    }

    void fromJson(const QJsonObject& obj) {
        templateName = obj["templateName"].toString();
        templateImagePath = obj["templateImagePath"].toString();
        templateJsonPath = obj["templateJsonPath"].toString();
    }
};

/**
 * @brief ROI模板（方案中的ROI配置，使用归一化坐标）
 *
 * 将 ROI 区域和检测项配置以相对坐标保存，
 * 使得方案可以适配不同分辨率的图片。
 */
struct RoiTemplate
{
    QString name;                       ///< ROI名称
    QRectF relativeRect;                ///< 归一化坐标 (0~1，相对于参考图像尺寸)
    QList<DetectionItem> detectionItems; ///< 该ROI下的检测项列表
    PipelineConfig pipelineConfig;      ///< 该ROI的Pipeline配置
    QString color;                      ///< ROI显示颜色

    RoiTemplate() : color("#FF6B6B") {}

    /**
     * @brief 从 RoiConfig 创建（将绝对坐标转为归一化坐标）
     */
    static RoiTemplate fromRoiConfig(const RoiConfig& roiConfig, const QSize& refImageSize) {
        RoiTemplate rt;
        rt.name = roiConfig.roiName;
        rt.color = roiConfig.color;
        rt.pipelineConfig = roiConfig.pipelineConfig;
        rt.detectionItems = roiConfig.detectionItems;

        if (refImageSize.width() > 0 && refImageSize.height() > 0) {
            rt.relativeRect = QRectF(
                roiConfig.roiRect.x() / refImageSize.width(),
                roiConfig.roiRect.y() / refImageSize.height(),
                roiConfig.roiRect.width() / refImageSize.width(),
                roiConfig.roiRect.height() / refImageSize.height()
            );
        }

        return rt;
    }

    /**
     * @brief 还原为 RoiConfig（将归一化坐标转为绝对坐标）
     */
    RoiConfig toRoiConfig(const QSize& currentImageSize) const {
        RoiConfig rc;
        rc.roiName = name;
        rc.color = color;
        rc.pipelineConfig = pipelineConfig;
        rc.detectionItems = detectionItems;
        rc.roiId = generateUniqueId("roi");

        rc.roiRect = QRectF(
            relativeRect.x() * currentImageSize.width(),
            relativeRect.y() * currentImageSize.height(),
            relativeRect.width() * currentImageSize.width(),
            relativeRect.height() * currentImageSize.height()
        );

        return rc;
    }

    QJsonObject toJson() const {
        QJsonObject obj;
        obj["name"] = name;
        obj["color"] = color;

        QJsonObject rect;
        rect["x"] = relativeRect.x();
        rect["y"] = relativeRect.y();
        rect["width"] = relativeRect.width();
        rect["height"] = relativeRect.height();
        obj["relativeRect"] = rect;

        QJsonArray detections;
        for (const auto& item : detectionItems) {
            detections.append(item.toJson());
        }
        obj["detectionItems"] = detections;

        QJsonObject pipeConfig;
        pipeConfig["brightness"] = pipelineConfig.brightness;
        pipeConfig["contrast"] = pipelineConfig.contrast;
        pipeConfig["gamma"] = pipelineConfig.gamma;
        pipeConfig["sharpen"] = pipelineConfig.sharpen;
        pipeConfig["channel"] = static_cast<int>(pipelineConfig.channel);
        obj["pipelineConfig"] = pipeConfig;

        return obj;
    }

    void fromJson(const QJsonObject& obj) {
        name = obj["name"].toString();
        color = obj["color"].toString("#FF6B6B");

        QJsonObject rect = obj["relativeRect"].toObject();
        relativeRect = QRectF(
            rect["x"].toDouble(), rect["y"].toDouble(),
            rect["width"].toDouble(), rect["height"].toDouble()
        );

        detectionItems.clear();
        QJsonArray detections = obj["detectionItems"].toArray();
        for (const auto& val : detections) {
            DetectionItem item;
            item.fromJson(val.toObject());
            detectionItems.append(item);
        }

        QJsonObject pipeConfig = obj["pipelineConfig"].toObject();
        if (!pipeConfig.isEmpty()) {
            pipelineConfig.brightness = pipeConfig["brightness"].toInt(0);
            pipelineConfig.contrast = pipeConfig["contrast"].toDouble(1.0);
            pipelineConfig.gamma = pipeConfig["gamma"].toDouble(1.0);
            pipelineConfig.sharpen = pipeConfig["sharpen"].toDouble(1.0);
            pipelineConfig.channel = static_cast<PipelineConfig::Channel>(
                pipeConfig["channel"].toInt(0));
        }
    }
};

/**
 * @brief 检测方案
 *
 * 将 ROI 布局、检测项配置、Pipeline 参数和模板库
 * 封装为一个可保存、可加载、可复用的完整检测方案。
 *
 * 存储目录结构：
 *   resources/profiles/<profileName>/
 *   ├── profile.json          // 方案配置
 *   ├── thumbnail.png         // 方案缩略图（可选）
 *   └── templates/            // 模板库
 *       ├── 螺丝A.png
 *       ├── 螺丝A.json
 *       └── ...
 */
struct InspectionProfile
{
    QString profileId;                      ///< 方案唯一ID
    QString profileName;                    ///< 方案名称
    QString description;                    ///< 描述信息
    QDateTime createdAt;                    ///< 创建时间
    QDateTime updatedAt;                    ///< 最后修改时间
    QSize referenceImageSize;               ///< 参考图像尺寸（用于坐标归一化）
    QList<RoiTemplate> roiTemplates;        ///< ROI模板列表
    QList<TemplateEntry> templates;         ///< 模板库（按名称保存）
    PipelineConfig globalPipelineConfig;    ///< 全局Pipeline配置

    InspectionProfile()
        : createdAt(QDateTime::currentDateTime())
        , updatedAt(QDateTime::currentDateTime())
    {
        profileId = generateUniqueId("profile");
    }

    // ==================== 模板管理 ====================

    /**
     * @brief 添加模板到方案
     */
    void addTemplate(const TemplateEntry& entry) {
        // 同名模板替换
        for (int i = 0; i < templates.size(); ++i) {
            if (templates[i].templateName == entry.templateName) {
                templates[i] = entry;
                updatedAt = QDateTime::currentDateTime();
                return;
            }
        }
        templates.append(entry);
        updatedAt = QDateTime::currentDateTime();
    }

    /**
     * @brief 移除模板
     */
    bool removeTemplate(const QString& templateName) {
        for (int i = 0; i < templates.size(); ++i) {
            if (templates[i].templateName == templateName) {
                templates.removeAt(i);
                updatedAt = QDateTime::currentDateTime();
                return true;
            }
        }
        return false;
    }

    /**
     * @brief 根据名称查找模板
     */
    const TemplateEntry* findTemplate(const QString& templateName) const {
        for (const auto& t : templates) {
            if (t.templateName == templateName) {
                return &t;
            }
        }
        return nullptr;
    }

    /**
     * @brief 获取所有模板名称
     */
    QStringList templateNames() const {
        QStringList names;
        for (const auto& t : templates) {
            names.append(t.templateName);
        }
        return names;
    }

    // ==================== JSON 序列化 ====================

    QJsonObject toJson() const {
        QJsonObject obj;
        obj["profileId"] = profileId;
        obj["profileName"] = profileName;
        obj["description"] = description;
        obj["createdAt"] = createdAt.toString(Qt::ISODate);
        obj["updatedAt"] = updatedAt.toString(Qt::ISODate);
        obj["refImageWidth"] = referenceImageSize.width();
        obj["refImageHeight"] = referenceImageSize.height();

        // ROI模板列表
        QJsonArray roiArray;
        for (const auto& roi : roiTemplates) {
            roiArray.append(roi.toJson());
        }
        obj["roiTemplates"] = roiArray;

        // 模板库元数据（实际文件另存）
        QJsonArray tplArray;
        for (const auto& tpl : templates) {
            tplArray.append(tpl.toJson());
        }
        obj["templates"] = tplArray;

        // 全局Pipeline配置
        QJsonObject pipeConfig;
        pipeConfig["brightness"] = globalPipelineConfig.brightness;
        pipeConfig["contrast"] = globalPipelineConfig.contrast;
        pipeConfig["gamma"] = globalPipelineConfig.gamma;
        pipeConfig["sharpen"] = globalPipelineConfig.sharpen;
        pipeConfig["channel"] = static_cast<int>(globalPipelineConfig.channel);
        obj["globalPipelineConfig"] = pipeConfig;

        return obj;
    }

    void fromJson(const QJsonObject& obj) {
        profileId = obj["profileId"].toString();
        profileName = obj["profileName"].toString();
        description = obj["description"].toString();
        createdAt = QDateTime::fromString(obj["createdAt"].toString(), Qt::ISODate);
        updatedAt = QDateTime::fromString(obj["updatedAt"].toString(), Qt::ISODate);
        referenceImageSize = QSize(
            obj["refImageWidth"].toInt(0),
            obj["refImageHeight"].toInt(0)
        );

        roiTemplates.clear();
        QJsonArray roiArray = obj["roiTemplates"].toArray();
        for (const auto& val : roiArray) {
            RoiTemplate rt;
            rt.fromJson(val.toObject());
            roiTemplates.append(rt);
        }

        templates.clear();
        QJsonArray tplArray = obj["templates"].toArray();
        for (const auto& val : tplArray) {
            TemplateEntry te;
            te.fromJson(val.toObject());
            templates.append(te);
        }

        QJsonObject pipeConfig = obj["globalPipelineConfig"].toObject();
        if (!pipeConfig.isEmpty()) {
            globalPipelineConfig.brightness = pipeConfig["brightness"].toInt(0);
            globalPipelineConfig.contrast = pipeConfig["contrast"].toDouble(1.0);
            globalPipelineConfig.gamma = pipeConfig["gamma"].toDouble(1.0);
            globalPipelineConfig.sharpen = pipeConfig["sharpen"].toDouble(1.0);
            globalPipelineConfig.channel = static_cast<PipelineConfig::Channel>(
                pipeConfig["channel"].toInt(0));
        }
    }

    /**
     * @brief 保存方案到目录
     * @param profilesDir 方案根目录（如 resources/profiles）
     */
    bool saveToDirectory(const QString& profilesDir) {
        QString dirPath = profilesDir + "/" + profileName;
        QDir dir(dirPath);
        if (!dir.exists()) {
            dir.mkpath(".");
        }

        // 保存 profile.json
        QFile file(dirPath + "/profile.json");
        if (!file.open(QIODevice::WriteOnly)) {
            return false;
        }
        file.write(QJsonDocument(toJson()).toJson());
        file.close();

        // 保存模板文件
        for (auto& tpl : templates) {
            tpl.save(dirPath);
        }

        return true;
    }

    /**
     * @brief 从目录加载方案
     * @param profileDir 方案目录（如 resources/profiles/PCB检测）
     */
    bool loadFromDirectory(const QString& profileDir) {
        QFile file(profileDir + "/profile.json");
        if (!file.open(QIODevice::ReadOnly)) {
            return false;
        }
        QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
        file.close();

        fromJson(doc.object());

        // 加载模板图像
        for (auto& tpl : templates) {
            tpl.load(profileDir);
        }

        return true;
    }

    /**
     * @brief 生成缩略图（使用第一个ROI区域的图像）
     */
    cv::Mat generateThumbnail(const cv::Mat& fullImage, int thumbWidth = 120) const {
        if (fullImage.empty()) return cv::Mat();

        cv::Mat thumb;
        double scale = static_cast<double>(thumbWidth) / fullImage.cols;
        cv::resize(fullImage, thumb, cv::Size(), scale, scale, cv::INTER_AREA);
        return thumb;
    }
};