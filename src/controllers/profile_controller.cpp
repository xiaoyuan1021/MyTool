#include "profile_controller.h"
#include "core/profile_manager.h"
#include "core/roi_manager.h"
#include "core/pipeline_manager.h"
#include "widgets/tab_manager.h"
#include "controllers/roi_ui_controller.h"
#include "widgets/barcode_tab_widget.h"
#include "ui/toast_notification.h"
#include "logger.h"

#include <QMessageBox>
#include <QInputDialog>
#include <QLineEdit>

ProfileController::ProfileController(ProfileManager* profileManager,
                                     RoiManager* roiManager,
                                     PipelineManager* pipelineManager,
                                     TabManager* tabManager,
                                     RoiUiController* roiUiController,
                                     ToastNotification* toast,
                                     QWidget* parentWidget,
                                     QObject* parent)
    : QObject(parent)
    , m_profileManager(profileManager)
    , m_roiManager(roiManager)
    , m_pipelineManager(pipelineManager)
    , m_tabManager(tabManager)
    , m_roiUiController(roiUiController)
    , m_toast(toast)
    , m_parentWidget(parentWidget)
{
}

void ProfileController::saveToProfile()
{
    if (!m_profileManager) return;

    cv::Mat currentImage = m_roiManager->getFullImage();
    if (currentImage.empty()) {
        QMessageBox::warning(m_parentWidget, "提示", "请先打开图片");
        return;
    }

    bool ok;
    QString name = QInputDialog::getText(m_parentWidget, "保存检测方案", "方案名称:",
                                          QLineEdit::Normal, QString(), &ok);
    if (!ok || name.trimmed().isEmpty()) return;

    if (m_profileManager->saveCurrentAsProfile(name.trimmed())) {
        if (m_toast) m_toast->showMessage(QString("方案 '%1' 已保存").arg(name.trimmed()));
        Logger::instance()->info(QString("[ProfileController] 方案已保存: %1").arg(name.trimmed()));
    } else {
        QMessageBox::warning(m_parentWidget, "保存失败", "保存方案失败，请查看日志");
    }
}

void ProfileController::loadFromProfile()
{
    if (!m_profileManager) return;

    QList<ProfileManager::ProfileSummary> profiles = m_profileManager->getProfileList();
    if (profiles.isEmpty()) {
        QMessageBox::information(m_parentWidget, "提示", "没有已保存的检测方案\n请先点击「保存到方案」创建方案");
        return;
    }

    QStringList names;
    QList<QString> ids;
    for (const auto& p : profiles) {
        names.append(QString("%1 (ROI:%2, 模板:%3)").arg(p.profileName).arg(p.roiCount).arg(p.templateCount));
        ids.append(p.profileId);
    }

    bool ok = false;
    QString selected = QInputDialog::getItem(m_parentWidget, "加载检测方案", "选择方案:", names, 0, false, &ok);
    if (!ok) return;

    int idx = names.indexOf(selected);
    if (idx < 0) return;

    if (m_profileManager->loadProfile(ids[idx])) {
        m_roiUiController->syncRoiConfigsToWidget();

        // 同步条码配置
        syncBarcodeConfigFromRois();

        if (m_toast) m_toast->showMessage(QString("方案 '%1' 已加载").arg(profiles[idx].profileName));
        emit requestRefresh();
    } else {
        QMessageBox::warning(m_parentWidget, "加载失败", "加载方案失败，请确保已打开图片");
    }
}

void ProfileController::syncBarcodeConfigFromRois()
{
    QList<RoiConfig> rois = m_roiManager->getRoiConfigs();
    for (const auto& roi : rois) {
        bool hasBarcode = false;
        for (const auto& det : roi.detectionItems) {
            if (det.type == DetectionType::Barcode && det.enabled) {
                hasBarcode = true;
                break;
            }
        }
        if (hasBarcode) {
            PipelineConfig pc = m_pipelineManager->getConfigSnapshot();
            pc.barcode = roi.pipelineConfig.barcode;
            m_pipelineManager->setConfig(pc);
            auto* barcodeTab = m_tabManager->getBarcodeTab();
            if (barcodeTab) {
                barcodeTab->setBarcodeConfig(pc.barcode);
            }
            break;
        }
    }
}