#include "config_manager.h"
#include <QJsonArray>
#include <QFile>
#include <QStandardPaths>
#include <QDir>
#include <QCoreApplication>
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

    // 提取配置
    QJsonObject extractObj;
    extractObj["mode"] = (shapeFilterConfig.mode == FilterMode::And) ? "and" : "or";
    QJsonArray conditionsArray;
    for (const auto& cond : shapeFilterConfig.conditions) {
        QJsonObject condObj;
        condObj["feature"] = getFeatureName(cond.feature);
        condObj["minValue"] = cond.minValue;
        condObj["maxValue"] = cond.maxValue;
        conditionsArray.append(condObj);
    }
    extractObj["conditions"] = conditionsArray;
    json["extract"] = extractObj;

    // 判定配置
    QJsonObject judgeObj;
    judgeObj["minRegionCount"] = minRegionCount;
    judgeObj["maxRegionCount"] = maxRegionCount;
    judgeObj["currentRegionCount"] = currentRegionCount;

    json["judge"] = judgeObj;

    // 算法队列配置
    QJsonArray algorithmArray;
    for (const auto& step : algorithmQueue) {
        QJsonObject stepObj;
        stepObj["name"] = step.name;
        stepObj["type"] = step.type;
        stepObj["enabled"] = step.enabled;
        stepObj["description"] = step.description;
        
        QJsonObject paramsObj;
        for (auto it = step.params.begin(); it != step.params.end(); ++it) {
            paramsObj[it.key()] = QJsonValue::fromVariant(it.value());
        }
        stepObj["params"] = paramsObj;
        algorithmArray.append(stepObj);
    }
    json["algorithms"] = algorithmArray;

    // 多图片ROI配置
    json["currentImageId"] = currentImageId;
    QJsonObject multiRoiObj;
    for (auto it = multiRoiConfigs.constBegin(); it != multiRoiConfigs.constEnd(); ++it) {
        QJsonArray roiConfigsArray;
        for (const auto& config : it.value()) {
            roiConfigsArray.append(config.toJson());
        }
        multiRoiObj[it.key()] = roiConfigsArray;
    }
    json["multiRoiConfigs"] = multiRoiObj;

    // MQTT 配置
    json["mqtt"] = mqttConfig.toJson();

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

    // 提取配置
    if (json.contains("extract")) {
        QJsonObject extractObj = json["extract"].toObject();
        shapeFilterConfig.clear();

        QString modeStr = extractObj["mode"].toString("and");
        shapeFilterConfig.mode = (modeStr == "and") ? FilterMode::And : FilterMode::Or;

        QJsonArray conditionsArray = extractObj["conditions"].toArray();
        for (const auto& item : conditionsArray) {
            QJsonObject condObj = item.toObject();
            QString featureName = condObj["feature"].toString();
            double minValue = condObj["minValue"].toDouble(0.0);
            double maxValue = condObj["maxValue"].toDouble(1e18);

            ShapeFeature feature = ShapeFeature::Area;
            if (featureName == "circularity") feature = ShapeFeature::Circularity;
            else if (featureName == "width") feature = ShapeFeature::Width;
            else if (featureName == "height") feature = ShapeFeature::Height;
            else if (featureName == "compactness") feature = ShapeFeature::Compactness;
            else if (featureName == "convexity") feature = ShapeFeature::Convexity;
            else if (featureName == "anisometry") feature = ShapeFeature::RectangularityAnisometry;

            shapeFilterConfig.addCondition(FilterCondition(feature, minValue, maxValue));
        }
    }

    // 判定配置
    if (json.contains("judge")) 
    {
        QJsonObject judgeObj = json["judge"].toObject();
        minRegionCount = judgeObj["minRegionCount"].toInt(0);
        maxRegionCount = judgeObj["maxRegionCount"].toInt(1000);
        currentRegionCount = judgeObj["currentRegionCount"].toInt(0);
    }

    // 算法队列配置
    if (json.contains("algorithms")) 
    {
        algorithmQueue.clear();
        QJsonArray algorithmArray = json["algorithms"].toArray();
        for (const auto& item : algorithmArray) {
            QJsonObject stepObj = item.toObject();
            AlgorithmStep step;
            step.name = stepObj["name"].toString();
            step.type = stepObj["type"].toString();
            step.enabled = stepObj["enabled"].toBool(true);
            step.description = stepObj["description"].toString();
            
            QJsonObject paramsObj = stepObj["params"].toObject();
            for (auto it = paramsObj.begin(); it != paramsObj.end(); ++it) {
                step.params[it.key()] = it.value().toVariant();
            }
            algorithmQueue.append(step);
        }
    }

    // 多图片ROI配置
    if (json.contains("currentImageId")) {
        currentImageId = json["currentImageId"].toString();
    }
    if (json.contains("multiRoiConfigs")) {
        multiRoiConfigs.clear();
        QJsonObject multiRoiObj = json["multiRoiConfigs"].toObject();
        for (auto it = multiRoiObj.begin(); it != multiRoiObj.end(); ++it) {
            QList<RoiConfig> configs;
            QJsonArray configsArray = it.value().toArray();
            for (const auto& val : configsArray) {
                RoiConfig config;
                config.fromJson(val.toObject());
                configs.append(config);
            }
            multiRoiConfigs[it.key()] = configs;
        }
    }

    // MQTT 配置
    if (json.contains("mqtt")) {
        mqttConfig.fromJson(json["mqtt"].toObject());
    }
}

ConfigManager& ConfigManager::instance()
{
    static ConfigManager s_instance;
    return s_instance;
}

bool ConfigManager::saveConfig(const AppConfig& config, const QString& filePath)
{
    try 
    {
        QJsonObject json = config.toJson();
        QJsonDocument doc(json);

        QFile file(filePath);
        if (!file.open(QIODevice::WriteOnly)) 
        {
            Logger::instance()->error(QString("无法打开配置文件: %1").arg(filePath));
            return false;
        }

        file.write(doc.toJson());
        file.close();

        Logger::instance()->info(QString("配置已保存: %1").arg(filePath));
        return true;
    } 
    catch (const std::exception& e) 
    {
        Logger::instance()->error(QString("保存配置失败: %1").arg(e.what()));
        return false;
    }
}

bool ConfigManager::loadConfig(AppConfig& config, const QString& filePath)
{
    try 
    {
        QFile file(filePath);
        if (!file.exists()) 
        {
            Logger::instance()->warning(QString("配置文件不存在: %1").arg(filePath));
            return false;
        }

        if (!file.open(QIODevice::ReadOnly)) 
        {
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
    } 
    catch (const std::exception& e) 
    {
        Logger::instance()->error(QString("加载配置失败: %1").arg(e.what()));
        return false;
    }
}

QString ConfigManager::getDefaultConfigPath() const
{
    //QString configDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QString appPath = QCoreApplication::applicationDirPath();
    QString rawPath = appPath + "/../../app_config.json";
    return QDir::cleanPath(rawPath);
}
