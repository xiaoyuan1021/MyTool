#pragma once

#include <QString>
#include <QRectF>
#include <QMap>
#include <QJsonObject>
#include <QJsonDocument>
#include <QVector>
#include "roi_config.h"
#include "shape_filter_types.h"
#include "pipeline_config.h"
#include "mqtt_config.h"
#include "../algorithm/image_processor.h"
#include "../core/pipeline_types.h"

struct AppConfig
{
    // ========== 核心配置（统一为 PipelineConfig）==========
    PipelineConfig pipelineConfig;   ///< 所有 Pipeline 相关配置（增强/过滤/提取/判定/条码）

    // ========== 元数据（不属于 Pipeline）==========
    QRectF roiRect;                  ///< ROI 配置（单ROI模式，向后兼容）
    QJsonObject roiExportData;       ///< RoiManager 导出的完整配置（保持原始 JSON 格式）
    QStringList imageFilePaths;      ///< 图片文件路径列表（有序）
    QMap<QString, QString> imageIdToFilePath; ///< imageId -> filePath 映射
    QVector<AlgorithmStep> algorithmQueue;  ///< 算法队列（独立于 PipelineConfig）
    MqttConfig mqttConfig;           ///< MQTT 配置

    // 转换为 JSON
    QJsonObject toJson() const;
    // 从 JSON 加载
    void fromJson(const QJsonObject& json);
};

class ConfigManager
{
public:
    static ConfigManager& instance();

    // 保存配置到文件
    bool saveConfig(const AppConfig& config, const QString& filePath);

    // 从文件加载配置
    bool loadConfig(AppConfig& config, const QString& filePath);

    // 获取默认配置文件路径
    QString getDefaultConfigPath() const;

private:
    ConfigManager() = default;
    ~ConfigManager() = default;

    ConfigManager(const ConfigManager&) = delete;
    ConfigManager& operator=(const ConfigManager&) = delete;
};
