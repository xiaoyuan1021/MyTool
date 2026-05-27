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

    // Pipeline 配置（统一序列化）
    QJsonObject pipelineObj = pipelineConfig.toJson();
    // 序列化算法队列
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
    pipelineObj["algorithms"] = algorithmArray;
    json["pipeline"] = pipelineObj;

    // RoiManager 导出数据（保持原始 JSON 格式）
    if (!roiExportData.isEmpty()) {
        json["roiExport"] = roiExportData;
    }

    // 图片文件路径列表
    QJsonArray imagePathsArray;
    for (const auto& path : imageFilePaths) {
        imagePathsArray.append(path);
    }
    json["imagePaths"] = imagePathsArray;

    // imageId -> filePath 映射
    if (!imageIdToFilePath.isEmpty()) {
        QJsonObject mappingObj;
        for (auto it = imageIdToFilePath.constBegin(); it != imageIdToFilePath.constEnd(); ++it) {
            mappingObj[it.key()] = it.value();
        }
        json["imageIdToFilePath"] = mappingObj;
    }

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

    // ========== Pipeline 配置 ==========
    if (json.contains("pipeline")) {
        QJsonObject pipelineObj = json["pipeline"].toObject();
        pipelineConfig.fromJson(pipelineObj);

        // 算法队列
        if (pipelineObj.contains("algorithms")) {
            algorithmQueue.clear();
            QJsonArray algorithmArray = pipelineObj["algorithms"].toArray();
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
    }

    // RoiManager 导出数据
    if (json.contains("roiExport")) {
        roiExportData = json["roiExport"].toObject();
    }

    // 图片文件路径列表
    if (json.contains("imagePaths")) {
        imageFilePaths.clear();
        QJsonArray imagePathsArray = json["imagePaths"].toArray();
        for (const auto& val : imagePathsArray) {
            imageFilePaths.append(val.toString());
        }
    }

    // imageId -> filePath 映射
    if (json.contains("imageIdToFilePath")) {
        imageIdToFilePath.clear();
        QJsonObject mappingObj = json["imageIdToFilePath"].toObject();
        for (auto it = mappingObj.begin(); it != mappingObj.end(); ++it) {
            imageIdToFilePath[it.key()] = it.value().toString();
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
    // 使用 CMake 编译时定义的项目根目录
    return QString(PROJECT_ROOT_DIR) + "/app_config.json";
}
