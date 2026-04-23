#include "controllers/config_controller.h"
#include "logger.h"
#include "widgets/enhance_tab_widget.h"
#include "widgets/filter_tab_widget.h"
#include "widgets/extract_tab_widget.h"
#include "widgets/judge_tab_widget.h"
#include "widgets/process_tab_widget.h"

#include <QFileDialog>
#include <QMessageBox>

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
    
    // 收集多图片ROI配置
    config.currentImageId = m_roiManager.getCurrentImageId();
    const QStringList imageIds = m_roiManager.getImageIds();
    for (const QString& imageId : imageIds) {
        // 临时切换到该图片获取其配置
        // 但这里我们不需要切换，因为RoiManager内部已经按图片ID存储了配置
    }
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
    // 应用 ROI 配置（单ROI模式，向后兼容）
    if (config.roiRect.isValid()) {
        m_roiManager.setRoi(config.roiRect);
        Logger::instance()->info("已恢复 ROI 配置");
    }
    
    // 应用多图片ROI配置
    if (!config.multiRoiConfigs.isEmpty()) {
        // 构建JSON文档用于导入
        QJsonObject root;
        root["version"] = "1.0";
        root["currentImageId"] = config.currentImageId;
        
        QJsonObject imagesObj;
        for (auto it = config.multiRoiConfigs.constBegin(); it != config.multiRoiConfigs.constEnd(); ++it) {
            QJsonObject imageObj;
            QJsonArray configsArray;
            for (const auto& cfg : it.value()) {
                configsArray.append(cfg.toJson());
            }
            imageObj["roiConfigs"] = configsArray;
            imageObj["selectedRoiId"] = "";
            imagesObj[it.key()] = imageObj;
        }
        root["images"] = imagesObj;
        
        m_roiManager.importAllConfigsFromJson(QJsonDocument(root));
        Logger::instance()->info(QString("已恢复多图片ROI配置，共%1张图片").arg(config.multiRoiConfigs.size()));
    }
    
    // 应用增强参数
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