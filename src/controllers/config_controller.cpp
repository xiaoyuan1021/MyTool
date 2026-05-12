#include "controllers/config_controller.h"
#include "widgets/i_configurable_tab.h"
#include "logger.h"
#include "utils/path_utils.h"

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
{
}

void ConfigController::registerTab(IConfigurableTab* tab)
{
    if (tab && !m_configurableTabs.contains(tab)) {
        m_configurableTabs.append(tab);
    }
}

void ConfigController::registerTabs(const QList<IConfigurableTab*>& tabs)
{
    for (auto* tab : tabs) {
        registerTab(tab);
    }
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
    // 通过 IConfigurableTab 接口收集所有已注册 Tab 的配置到 PipelineConfig
    for (auto* tab : m_configurableTabs) {
        tab->saveToConfig(config.pipelineConfig);
    }

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

    // 收集算法队列（来自 PipelineManager，非 Tab）- 保留独立存储
    config.algorithmQueue = m_pipelineManager->getAlgorithmQueue();
}

void ConfigController::applyConfigToUI(const AppConfig& config)
{
    // ====== 第一步：如果配置包含图片路径，先加载图片 ======
    QMap<QString, QString> filePathToNewId;

    if (!config.imageFilePaths.isEmpty()) {
        m_roiManager.clear();

        int loadedCount = 0;
        for (const QString& filePath : config.imageFilePaths) {
            cv::Mat img = PathUtils::readImageFromFile(filePath, cv::IMREAD_COLOR);
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
        QMap<QString, QString> oldToNewId;
        for (auto it = config.imageIdToFilePath.constBegin(); it != config.imageIdToFilePath.constEnd(); ++it) {
            if (filePathToNewId.contains(it.value())) {
                oldToNewId[it.key()] = filePathToNewId[it.value()];
            }
        }

        if (oldToNewId.isEmpty()) {
            QStringList newIds = m_roiManager.getImageIds();
            QJsonObject images = config.roiExportData["images"].toObject();
            auto imgIt = images.begin();
            for (int i = 0; i < newIds.size() && imgIt != images.end(); ++i, ++imgIt) {
                oldToNewId[imgIt.key()] = newIds[i];
            }
        }

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

    // ====== 第四步：通过 IConfigurableTab 接口将配置应用到所有已注册 Tab ======
    for (auto* tab : m_configurableTabs) {
        tab->loadFromConfig(config.pipelineConfig);
    }

    // ====== 第五步：应用算法队列到 PipelineManager ======
    m_pipelineManager->clearAlgorithmQueue();
    for (const auto& step : config.algorithmQueue) {
        m_pipelineManager->addAlgorithmStep(step);
    }

    // 同步Pipeline中的提取配置
    m_pipelineManager->setShapeFilterConfig(config.pipelineConfig.shapeFilter);

    // 应用 PipelineConfig 到 PipelineManager（不含 algorithmQueue）
    m_pipelineManager->setConfig(config.pipelineConfig);

    // 发出配置已应用信号
    emit configApplied();
}
