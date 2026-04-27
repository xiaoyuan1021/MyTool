#pragma once

#include <QString>
#include <QRectF>
#include <QMap>
#include <QJsonObject>
#include <QJsonDocument>
#include <QVector>
#include "roi_config.h"
#include "shape_filter_types.h"
#include "mqtt_config.h"
#include "../algorithm/image_processor.h"
#include "../core/pipeline_types.h"

struct AppConfig
{
    // ROI 配置（单ROI模式，向后兼容）
    QRectF roiRect;
    //cv::Rect roiRect;
    // 增强参数
    int brightness = 0;
    int contrast = 100;
    int gamma = 100;
    int sharpen = 100;

    // 过滤配置
    int filterMode = 0;
    int grayLow = 0;
    int grayHigh = 255;
    bool enableFilter = false;
    // 算法队列配置
    QVector<AlgorithmStep> algorithmQueue;

    // 提取配置
    ShapeFilterConfig shapeFilterConfig;

    // 判定配置
    int minRegionCount = 0;
    int maxRegionCount = 1000;
    int currentRegionCount = 0;

    // RoiManager 导出的完整配置（保持原始 JSON 格式，避免序列化往返）
    QJsonObject roiExportData;

    // 图片文件路径列表（有序，用于导入时重新加载图片）
    QStringList imageFilePaths;

    // imageId -> filePath 映射（用于导入时重建 imageId 对应关系）
    QMap<QString, QString> imageIdToFilePath;

    // MQTT 配置
    MqttConfig mqttConfig;

    // 条码识别配置
    BarcodeConfig barcodeConfig;

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
