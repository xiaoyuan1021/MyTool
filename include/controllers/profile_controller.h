#ifndef PROFILE_CONTROLLER_H
#define PROFILE_CONTROLLER_H

#include <QObject>
#include <QWidget>

class ProfileManager;
class RoiManager;
class PipelineManager;
class TabManager;
class RoiUiController;
class ToastNotification;

/**
 * 方案控制器 - 负责检测方案的保存和加载业务逻辑
 *
 * 职责：
 * 1. 保存当前检测方案
 * 2. 加载检测方案并同步ROI/条码配置
 *
 * 从 MainWindow 中提取，降低 MainWindow 的职责复杂度。
 */
class ProfileController : public QObject
{
    Q_OBJECT

public:
    explicit ProfileController(ProfileManager* profileManager,
                               RoiManager* roiManager,
                               PipelineManager* pipelineManager,
                               TabManager* tabManager,
                               RoiUiController* roiUiController,
                               ToastNotification* toast,
                               QWidget* parentWidget,
                               QObject* parent = nullptr);

public slots:
    /// 保存当前配置为检测方案
    void saveToProfile();
    /// 加载检测方案
    void loadFromProfile();

signals:
    /// 加载方案后需要刷新Pipeline
    void requestRefresh();

private:
    /// 从已加载的ROI中同步条码配置到PipelineManager和BarcodeTab
    void syncBarcodeConfigFromRois();

    ProfileManager* m_profileManager;
    RoiManager* m_roiManager;
    PipelineManager* m_pipelineManager;
    TabManager* m_tabManager;
    RoiUiController* m_roiUiController;
    ToastNotification* m_toast;
    QWidget* m_parentWidget;
};

#endif // PROFILE_CONTROLLER_H