#include "controllers/config_controller.h"
#include "logger.h"
#include "widgets/enhance_tab_widget.h"
#include "widgets/filter_tab_widget.h"
#include "widgets/extract_tab_widget.h"
#include "widgets/judge_tab_widget.h"
#include "widgets/process_tab_widget.h"
#include "widgets/barcode_tab_widget.h"

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
    , m_barcodeTabWidget(nullptr)
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
    ProcessTabWidget* processTab,
    BarcodeTabWidget* barcodeTab)
{
    m_enhanceTabWidget = enhanceTab;
    m_filterTabWidget = filterTab;
    m_extractTabWidget = extractTab;
    m_judgeTabWidget = judgeTab;
    m_processTabWidget = processTab;
    m_barcodeTabWidget = barcodeTab;
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
    
    // 收集多图片ROI配置（保持原始 JSON 格式，避免序列化往返）
    config.roiExportData = m_roiManager.exportAllConfigsToJson().object();
    
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

    // 收集条码识别配置（从 Widget 直接收集，与其他 Tab 一致）
    if (m_barcodeTabWidget) {
        config.barcodeConfig = m_barcodeTabWidget->getBarcodeConfig();
    } else {
        config.barcodeConfig = m_pipelineManager->getConfigSnapshot().barcode;
    }
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
    if (!config.roiExportData.isEmpty()) {
        // 通过 filePath 桥接 oldImageId → newImageId
        QMap<QString, QString> oldToNewId;
        for (auto it = config.imageIdToFilePath.constBegin(); it != config.imageIdToFilePath.constEnd(); ++it) {
            if (filePathToNewId.contains(it.value())) {
                oldToNewId[it.key()] = filePathToNewId[it.value()];
            }
        }

        // 旧配置无 imageIdToFilePath 时按顺序匹配
        if (oldToNewId.isEmpty()) {
            QStringList newIds = m_roiManager.getImageIds();
            QJsonObject images = config.roiExportData["images"].toObject();
            auto imgIt = images.begin();
            for (int i = 0; i < newIds.size() && imgIt != images.end(); ++i, ++imgIt) {
                oldToNewId[imgIt.key()] = newIds[i];
            }
        }

        // 重构 JSON：用新 imageId 替换旧 imageId
        QJsonObject exportJson = config.roiExportData;
        QJsonObject oldImages = exportJson["images"].toObject();
        QJsonObject newImages;
        for (auto it = oldImages.begin(); it != oldImages.end(); ++it) {
            newImages[oldToNewId.value(it.key(), it.key())] = it.value();
        }
        exportJson["images"] = newImages;

        QString oldId = exportJson["currentImageId"].toString();
        exportJson["currentImageId"] = oldToNewId.value(oldId, oldId);

        m_roiManager.importAllConfigsFromJson(QJsonDocument(exportJson));
        Logger::instance()->info("已恢复多图片ROI配置");
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

    // 恢复条码识别配置
    if (m_barcodeTabWidget) {
        m_barcodeTabWidget->setBarcodeConfig(config.barcodeConfig);
    } else {
        PipelineConfig pc = m_pipelineManager->getConfigSnapshot();
        pc.barcode = config.barcodeConfig;
        m_pipelineManager->setConfig(pc);
    }

    // 发出配置已应用信号
    emit configApplied();
}