#pragma once

#include <QString>
#include <QRectF>
#include <QList>
#include <QJsonObject>
#include <QDateTime>
#include <QSize>
#include <opencv2/core.hpp>
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
    QString templateFilePath;    ///< 模板文件相对路径（相对于方案目录）
    cv::Mat templateImage;       ///< 模板图像（运行时加载，不序列化到 JSON）
    TemplateParams params;       ///< 模板匹配参数

    TemplateEntry() : params() {
        params.matchMethod = 5; // TM_CCOEFF_NORMED 默认值
    }

    TemplateEntry(const QString& name)
        : templateName(name), params() {
        params.matchMethod = 5;
    }

    bool load(const QString& baseDir);
    bool save(const QString& baseDir);
    QJsonObject toJson() const;
    void fromJson(const QJsonObject& obj);
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

    static RoiTemplate fromRoiConfig(const RoiConfig& roiConfig, const QSize& refImageSize);
    RoiConfig toRoiConfig(const QSize& currentImageSize) const;
    QJsonObject toJson() const;
    void fromJson(const QJsonObject& obj);
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

    // 模板管理
    void addTemplate(const TemplateEntry& entry);
    bool removeTemplate(const QString& templateName);
    const TemplateEntry* findTemplate(const QString& templateName) const;
    QStringList templateNames() const;

    // JSON 序列化
    QJsonObject toJson() const;
    void fromJson(const QJsonObject& obj);

    // 文件系统操作
    bool saveToDirectory(const QString& profilesDir);
    bool loadFromDirectory(const QString& profileDir);
    cv::Mat generateThumbnail(const cv::Mat& fullImage, int thumbWidth = 120) const;
};