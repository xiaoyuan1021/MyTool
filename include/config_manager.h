#pragma once

#include <QString>
#include <QRect>
#include <QJsonObject>
#include <QJsonDocument>

struct AppConfig
{
    // ROI 配置
    QRectF roiRect;

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

    // 判定配置
    int minRegionCount = 0;
    int maxRegionCount = 1000;
    int currentRegionCount = 0;

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
