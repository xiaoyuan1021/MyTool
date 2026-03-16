#include "config_manager.h"
#include <QJsonArray>
#include <QFile>
#include <QStandardPaths>
#include <QDir>
#include "logger.h"

QJsonObject AppConfig::toJson() const
{
    QJsonObject json;

    // ROI
    QJsonObject roiObj;
    roiObj["x"] = roiRect.x();
    roiObj["y"] = roiRect.y();
    roiObj["width"] = roiRect.width();
    roiObj["height"] = roiRect.height();
    json["roi"] = roiObj;

    // 增强参数
    QJsonObject enhanceObj;
    enhanceObj["brightness"] = brightness;
    enhanceObj["contrast"] = contrast;
    enhanceObj["gamma"] = gamma;
    enhanceObj["sharpen"] = sharpen;
    json["enhance"] = enhanceObj;

    // 过滤配置
    QJsonObject filterObj;
    filterObj["mode"] = filterMode;
    filterObj["grayLow"] = grayLow;
    filterObj["grayHigh"] = grayHigh;
    filterObj["enabled"] = enableFilter;
    json["filter"] = filterObj;

    // 判定配置
    QJsonObject judgeObj;
    judgeObj["minRegionCount"] = minRegionCount;
    judgeObj["maxRegionCount"] = maxRegionCount;
    json["judge"] = judgeObj;

    return json;
}

void AppConfig::fromJson(const QJsonObject& json)
{
    // ROI
    if (json.contains("roi")) {
        QJsonObject roiObj = json["roi"].toObject();
        roiRect = QRectF(roiObj["x"].toDouble(),
                        roiObj["y"].toDouble(),
                        roiObj["width"].toDouble(),
                        roiObj["height"].toDouble());
    }

    // 增强参数
    if (json.contains("enhance")) {
        QJsonObject enhanceObj = json["enhance"].toObject();
        brightness = enhanceObj["brightness"].toInt(0);
        contrast = enhanceObj["contrast"].toInt(100);
        gamma = enhanceObj["gamma"].toInt(100);
        sharpen = enhanceObj["sharpen"].toInt(100);
    }

    // 过滤配置
    if (json.contains("filter")) {
        QJsonObject filterObj = json["filter"].toObject();
        filterMode = filterObj["mode"].toInt(0);
        grayLow = filterObj["grayLow"].toInt(0);
        grayHigh = filterObj["grayHigh"].toInt(255);
        enableFilter = filterObj["enabled"].toBool(false);
    }

    // 判定配置
    if (json.contains("judge")) {
        QJsonObject judgeObj = json["judge"].toObject();
        minRegionCount = judgeObj["minRegionCount"].toInt(0);
        maxRegionCount = judgeObj["maxRegionCount"].toInt(1000);
    }
}

ConfigManager& ConfigManager::instance()
{
    static ConfigManager s_instance;
    return s_instance;
}

bool ConfigManager::saveConfig(const AppConfig& config, const QString& filePath)
{
    try {
        QJsonObject json = config.toJson();
        QJsonDocument doc(json);

        QFile file(filePath);
        if (!file.open(QIODevice::WriteOnly)) {
            Logger::instance()->error(QString("无法打开配置文件: %1").arg(filePath));
            return false;
        }

        file.write(doc.toJson());
        file.close();

        Logger::instance()->info(QString("配置已保存: %1").arg(filePath));
        return true;
    } catch (const std::exception& e) {
        Logger::instance()->error(QString("保存配置失败: %1").arg(e.what()));
        return false;
    }
}

bool ConfigManager::loadConfig(AppConfig& config, const QString& filePath)
{
    try {
        QFile file(filePath);
        if (!file.exists()) {
            Logger::instance()->warning(QString("配置文件不存在: %1").arg(filePath));
            return false;
        }

        if (!file.open(QIODevice::ReadOnly)) {
            Logger::instance()->error(QString("无法打开配置文件: %1").arg(filePath));
            return false;
        }

        QByteArray data = file.readAll();
        file.close();

        QJsonDocument doc = QJsonDocument::fromJson(data);
        if (!doc.isObject()) {
            Logger::instance()->error("配置文件格式错误");
            return false;
        }

        config.fromJson(doc.object());
        Logger::instance()->info(QString("配置已加载: %1").arg(filePath));
        return true;
    } catch (const std::exception& e) {
        Logger::instance()->error(QString("加载配置失败: %1").arg(e.what()));
        return false;
    }
}

QString ConfigManager::getDefaultConfigPath() const
{
    QString configDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(configDir);
    return configDir + "/app_config.json";
}
