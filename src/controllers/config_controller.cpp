#include "controllers/config_controller.h"
#include "logger.h"
#include "widgets/enhance_tab_widget.h"
#include "widgets/filter_tab_widget.h"
#include "widgets/extract_tab_widget.h"
#include "widgets/judge_tab_widget.h"
#include "widgets/process_tab_widget.h"

#include <QFileDialog>
#include <QMessageBox>
#include <QFileInfo>
#include <opencv2/opencv.hpp>

ConfigController::ConfigController(
    PipelineManager* pipelineManager,
    RoiManager& roiManager,
    QObject* parent)
    : QObject(parent)
    , m_pipelineManager(pipelineManager)
    , m_roiManager(roiManager)
    , m_enhanceTabWidget(nullptr)
    , m_filterTabWidget(nullptr)
    , m_extractTabWidget(nullptr)
    , m_judgeTabWidget(nullptr)
    , m_processTabWidget(nullptr)
{
}

ConfigController::~ConfigController()
{
}

void ConfigController::setTabWidgets(
    EnhanceTabWidget* enhanceTab,
    FilterTabWidget* filterTab,
    ExtractTabWidget* extractTab,
    JudgeTabWidget* judgeTab,
    ProcessTabWidget* processTab)
{
    m_enhanceTabWidget = enhanceTab;
    m_filterTabWidget = filterTab;
    m_extractTabWidget = extractTab;
    m_judgeTabWidget = judgeTab;
    m_processTabWidget = processTab;
}

void ConfigController::saveConfig(QWidget* parentWidget)
{
    QString filePath = QFileDialog::getSaveFileName(
        parentWidget, 
        "保存配置", 
        "", 
        "JSON Files (*.json)"
    );
    
    if (filePath.isEmpty()) return;
    
    AppConfig config;
    collectConfigFromUI(config);
    
    if (ConfigManager::instance().saveConfig(config, filePath)) {
        Logger::instance()->info("配置已保存到: " + filePath);
        QMessageBox::information(parentWidget, "成功", "配置已保存");
    } else {
        Logger::instance()->error("配置保存失败");
        QMessageBox::warning(parentWidget, "错误", "配置保存失败");
    }
}

void ConfigController::loadConfig(QWidget* parentWidget)
{
    QString configPath = ConfigManager::instance().getDefaultConfigPath();
    QString filePath = QFileDialog::getOpenFileName(
        parentWidget,
        "导入配置",
        configPath,
        "JSON Files (*.json)"
    );
    
    if (filePath.isEmpty()) return;
    
    AppConfig config;
    if (ConfigManager::instance().loadConfig(config, filePath)) {
        applyConfigToUI(config);
        Logger::instance()->info("配置已从: " + filePath + " 加载");
        QMessageBox::information(parentWidget, "成功", "配置已加载");
    } else {
        Logger::instance()->error("配置加载失败");
        QMessageBox::warning(parentWidget, "错误", "配置加载失败");
    }
}

void ConfigController::collectConfigFromUI(AppConfig& config)
{
    // 收集 ROI 配置（单ROI模式，向后兼容）
    if (m_roiManager.isRoiActive()) {
        cv::Rect roi = m_roiManager.getLastRoi();
        config.roiRect = QRectF(
            static_cast<qreal>(roi.x), 
            static_cast<qreal>(roi.y), 
            static_cast<qreal>(roi.width), 
            static_cast<qreal>(roi.height)
        );
    }
    
    // 收集图片文件路径和 imageId -> filePath 映射
    config.imageFilePaths.clear();
    config.imageIdToFilePath.clear();
    const QStringList imageIds = m_roiManager.getImageIds();
    for (const QString& imageId : imageIds) {
        QString filePath = m_roiManager.getImageFilePath(imageId);
        if (!filePath.isEmpty()) {
            config.imageFilePaths.append(filePath);
            config.imageIdToFilePath[imageId] = filePath;
        }
    }
    
    // 收集多图片ROI配置
    config.currentImageId = m_roiManager.getCurrentImageId();
    // 直接从RoiManager导出所有配置
    QJsonDocument allConfigsDoc = m_roiManager.exportAllConfigsToJson();
    QJsonObject allConfigsObj = allConfigsDoc.object();
    if (allConfigsObj.contains("images")) {
        QJsonObject imagesObj = allConfigsObj["images"].toObject();
        for (auto it = imagesObj.begin(); it != imagesObj.end(); ++it) {
            QString imageId = it.key();
            QJsonObject imageObj = it.value().toObject();
            QList<RoiConfig> configs;
            QJsonArray configsArray = imageObj["roiConfigs"].toArray();
            for (const auto& val : configsArray) {
                RoiConfig cfg;
                cfg.fromJson(val.toObject());
                configs.append(cfg);
            }
            config.multiRoiConfigs[imageId] = configs;
        }
    }
    
    // 收集增强参数
    if (m_enhanceTabWidget) {
        m_enhanceTabWidget->getEnhanceConfig(
            config.brightness, 
            config.contrast,
            config.gamma, 
            config.sharpen
        );
    }
    
    // 收集过滤配置
    if (m_filterTabWidget) {
        m_filterTabWidget->getFilterConfig(
            config.filterMode, 
            config.grayLow, 
            config.grayHigh
        );
    }
    
    // 收集提取配置
    if (m_extractTabWidget) {
        m_extractTabWidget->getExtractConfig(config.shapeFilterConfig);
    }
    
    // 收集判定配置
    if (m_judgeTabWidget) {
        m_judgeTabWidget->getJudgeConfig(
            config.minRegionCount, 
            config.maxRegionCount, 
            config.currentRegionCount
        );
    }
    
    // 收集算法队列
    config.algorithmQueue = m_pipelineManager->getAlgorithmQueue();
}

void ConfigController::applyConfigToUI(const AppConfig& config)
{
    // ====== 第一步：如果配置包含图片路径，先加载图片 ======
    // 建立 filePath -> newImageId 的映射
    QMap<QString, QString> filePathToNewId;
    
    if (!config.imageFilePaths.isEmpty()) {
        // 清空现有图片
        m_roiManager.clear();
        
        int loadedCount = 0;
        for (const QString& filePath : config.imageFilePaths) {
            cv::Mat img = cv::imread(filePath.toStdString());
            if (img.empty()) {
                Logger::instance()->warning(QString("无法加载图片: %1").arg(filePath));
                continue;
            }
            QFileInfo fi(filePath);
            QString imageId = m_roiManager.addImage(img, fi.fileName(), filePath);
            if (!imageId.isEmpty()) {
                loadedCount++;
                filePathToNewId[filePath] = imageId;
            }
        }
        
        // 切换到第一张图片
        const QStringList imageIds = m_roiManager.getImageIds();
        if (!imageIds.isEmpty()) {
            m_roiManager.switchToImage(imageIds.first());
        }
        
        Logger::instance()->info(QString("已从配置加载 %1/%2 张图片")
            .arg(loadedCount).arg(config.imageFilePaths.size()));
    }
    
    // ====== 第二步：恢复单ROI配置（向后兼容） ======
    if (config.roiRect.isValid() && m_roiManager.imageCount() > 0) {
        m_roiManager.setRoi(config.roiRect);
        Logger::instance()->info("已恢复 ROI 配置");
    }
    
    // ====== 第三步：恢复多图片ROI配置 ======
    if (!config.multiRoiConfigs.isEmpty()) {
        // 关键修复：通过 imageIdToFilePath 映射，将旧 imageId 关联到新加载图片的 imageId
        const QStringList newImageIds = m_roiManager.getImageIds();
        
        // 构建 filePath -> newImageId 的映射
        QMap<QString, QString> filePathToNewId;
        for (const QString& newId : newImageIds) {
            QString filePath = m_roiManager.getImageFilePath(newId);
            if (!filePath.isEmpty()) {
                filePathToNewId[filePath] = newId;
            }
        }
        
        // 构建 oldImageId -> newImageId 的映射
        QMap<QString, QString> oldToNewId;
        for (auto it = config.imageIdToFilePath.constBegin(); it != config.imageIdToFilePath.constEnd(); ++it) {
            QString oldId = it.key();
            QString filePath = it.value();
            if (filePathToNewId.contains(filePath)) {
                oldToNewId[oldId] = filePathToNewId[filePath];
            }
        }
        
        // 如果没有 imageIdToFilePath 映射（旧格式配置文件），尝试按顺序匹配
        if (oldToNewId.isEmpty() && !config.multiRoiConfigs.isEmpty()) {
            auto oldIt = config.multiRoiConfigs.constBegin();
            for (int i = 0; i < newImageIds.size() && oldIt != config.multiRoiConfigs.constEnd(); ++i, ++oldIt) {
                oldToNewId[oldIt.key()] = newImageIds[i];
            }
        }
        
        // 构建 JSON 文档，使用新的 imageId
        QJsonObject root;
        root["version"] = "1.0";
        
        // 映射 currentImageId
        if (oldToNewId.contains(config.currentImageId)) {
            root["currentImageId"] = oldToNewId[config.currentImageId];
        } else {
            root["currentImageId"] = config.currentImageId;
        }
        
        QJsonObject imagesObj;
        for (auto it = config.multiRoiConfigs.constBegin(); it != config.multiRoiConfigs.constEnd(); ++it) {
            QString oldImageId = it.key();
            QString newImageId = oldToNewId.value(oldImageId, oldImageId);
            
            QJsonObject imageObj;
            QJsonArray configsArray;
            for (const auto& cfg : it.value()) {
                configsArray.append(cfg.toJson());
            }
            imageObj["roiConfigs"] = configsArray;
            imageObj["selectedRoiId"] = "";
            imagesObj[newImageId] = imageObj;
        }
        root["images"] = imagesObj;
        
        m_roiManager.importAllConfigsFromJson(QJsonDocument(root));
        Logger::instance()->info(QString("已恢复多图片ROI配置，共%1张图片").arg(config.multiRoiConfigs.size()));
    }
    
    // ====== 第四步：恢复增强参数 ======
    if (m_enhanceTabWidget) {
        m_enhanceTabWidget->setEnhanceConfig(
            config.brightness, 
            config.contrast,
            config.gamma, 
            config.sharpen
        );
    }
    
    // 应用过滤配置
    if (m_filterTabWidget) {
        m_filterTabWidget->setFilterConfig(
            config.filterMode, 
            config.grayLow, 
            config.grayHigh
        );
    }
    
    // 应用提取配置
    if (m_extractTabWidget) {
        m_extractTabWidget->setExtractConfig(config.shapeFilterConfig);
    }
    
    // 应用判定配置
    if (m_judgeTabWidget) {
        m_judgeTabWidget->setJudgeConfig(
            config.minRegionCount, 
            config.maxRegionCount, 
            config.currentRegionCount
        );
    }
    
    // 应用算法队列
    m_pipelineManager->clearAlgorithmQueue();
    for (const auto& step : config.algorithmQueue) {
        m_pipelineManager->addAlgorithmStep(step);
    }
    
    // 刷新ProcessTabWidget的UI列表显示
    if (m_processTabWidget) {
        m_processTabWidget->refreshAlgorithmListUI(config.algorithmQueue);
    }
    
    // 同步Pipeline中的提取配置
    m_pipelineManager->setShapeFilterConfig(config.shapeFilterConfig);
    
    // 发出配置已应用信号
    emit configApplied();
}