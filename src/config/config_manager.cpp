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
        // 新格式：统一在 "pipeline" 键下
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
    } else {
        // 旧格式：各字段平铺在顶层（向后兼容）
        // 增强参数
        if (json.contains("enhance")) {
            QJsonObject enhanceObj = json["enhance"].toObject();
            pipelineConfig.enhance.brightness = enhanceObj["brightness"].toInt(0);
            pipelineConfig.enhance.contrast = enhanceObj["contrast"].toInt(100) / 100.0;
            pipelineConfig.enhance.gamma = enhanceObj["gamma"].toInt(100) / 100.0;
            pipelineConfig.enhance.sharpen = enhanceObj["sharpen"].toInt(100) / 100.0;
        }

        // 过滤配置
        if (json.contains("filter")) {
            QJsonObject filterObj = json["filter"].toObject();
            pipelineConfig.colorFilter.currentFilterMode = static_cast<ImageFilterMode>(filterObj["mode"].toInt(0));
            pipelineConfig.colorFilter.grayLow = filterObj["grayLow"].toInt(0);
            pipelineConfig.colorFilter.grayHigh = filterObj["grayHigh"].toInt(255);
        }

        // 提取配置
        if (json.contains("extract")) {
            QJsonObject extractObj = json["extract"].toObject();
            pipelineConfig.shapeFilter.clear();
            QString modeStr = extractObj["mode"].toString("and");
            pipelineConfig.shapeFilter.mode = (modeStr == "and") ? FilterMode::And : FilterMode::Or;
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
                pipelineConfig.shapeFilter.addCondition(FilterCondition(feature, minValue, maxValue));
            }
        }

        // 判定配置
        if (json.contains("judge")) {
            QJsonObject judgeObj = json["judge"].toObject();
            pipelineConfig.judge.minRegionCount = judgeObj["minRegionCount"].toInt(0);
            pipelineConfig.judge.maxRegionCount = judgeObj["maxRegionCount"].toInt(1000);
            pipelineConfig.judge.currentRegionCount = judgeObj["currentRegionCount"].toInt(0);
        }

        // 算法队列配置
        if (json.contains("algorithms")) {
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

        // 条码识别配置
        if (json.contains("barcode")) {
            QJsonObject barcodeObj = json["barcode"].toObject();
            pipelineConfig.barcode.enableBarcode = barcodeObj["enableBarcode"].toBool(false);
            pipelineConfig.barcode.codeTypes.clear();
            QJsonArray codeTypesArray = barcodeObj["codeTypes"].toArray();
            for (const auto& val : codeTypesArray) {
                pipelineConfig.barcode.codeTypes.append(val.toString());
            }
            pipelineConfig.barcode.maxNumSymbols = barcodeObj["maxNumSymbols"].toInt(0);
        }
    }

    // RoiManager 导出数据
    if (json.contains("roiExport")) {
        roiExportData = json["roiExport"].toObject();
    } else if (json.contains("multiRoiConfigs")) {
        // 向后兼容：旧格式 multiRoiConfigs + currentImageId → 新格式
        QJsonObject exportObj;
        exportObj["version"] = "1.0";
        exportObj["currentImageId"] = json["currentImageId"].toString("");
        QJsonObject imagesObj;
        QJsonObject oldMultiRoi = json["multiRoiConfigs"].toObject();
        for (auto it = oldMultiRoi.begin(); it != oldMultiRoi.end(); ++it) {
            QJsonObject imageObj;
            imageObj["roiConfigs"] = it.value().toArray();
            imageObj["activeRoiId"] = "";
            imagesObj[it.key()] = imageObj;
        }
        exportObj["images"] = imagesObj;
        roiExportData = exportObj;
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
