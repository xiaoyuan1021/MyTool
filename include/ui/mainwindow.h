#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QTimer>
#include <QFutureWatcher>
#include <QDesktopServices>
#include <QPropertyAnimation>
#include <QGraphicsOpacityEffect>
#include <QParallelAnimationGroup>
#include <QSplashScreen>

#include <opencv2/opencv.hpp>

#include "pipeline_manager.h"
#include "roi_manager.h"
#include "core/mqtt_manager.h"
#include "ui/toast_notification.h"
#include "config/display_config.h"

// 前向声明
class QLabel;
class ImageView;
class SystemMonitor;
class FileManager;
class CloudDashboardManager;
class DisplayModeManager;
class ProfileController;
class TabManager;
class ImageListManager;
class RoiUiController;
class RoiDetectionConfigController;
class DetectionUiController;
class ConfigController;
class AutoDetectionController;
class PipelineResultHandler;
class ProfileManager;

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

/**
 * 主窗口 - 负责UI交互和显示
 *
 * 职责：
 * 1. UI事件处理
 * 2. 图像显示
 * 3. 协调各模块工作
 */

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

    /// 供外部模块请求处理
    void processAndDisplay();
    void showImage(const cv::Mat& img);
    /// 请求刷新Pipeline（防抖 + 脏标记），供外部模块调用
    void requestRefresh();

private slots:
    void on_btn_pipelineConfig_clicked();
    void on_btn_addROI_clicked();
    void on_btn_deleteROI_clicked();
    void on_btn_switchROI_clicked();
    void on_tabWidget_currentChanged(int index);
    void on_btn_Log_clicked();
    void on_btn_Home_clicked();
    void on_btn_saveConfig_clicked();
    void on_btn_importConfig_clicked();
    void on_btn_startAutoDetection_clicked();
    void startAutoDetection();
    void on_btn_stopAutoDetection_clicked();
    void on_btn_cloudPlatform_clicked();
    void on_btn_saveToProfile_clicked();
    void on_btn_loadFromProfile_clicked();
    void on_btn_importFolder_clicked();

private:
    // 初始化方法
    void setupBasicInfrastructure();
    void setupManagers();
    void setupControllers();
    void setupControllerConnections();
    void setupTabRegistration();
    void setupResultHandler();
    void setupMqtt();
    void setupUI();
    void setupConnections();
    void setupAnimations();

    /// 页面切换淡入淡出动画
    void animatePageSwitch(int fromIndex, int toIndex);
    /// 淡入效果
    void fadeIn(QWidget* widget, int durationMs = 300);
    /// 淡出效果
    void fadeOut(QWidget* widget, int durationMs = 200);

    void ensureTabExists(const QString& tabName);

    /// 安全停止视频（析构时调用，防止定时器回调访问已销毁的成员）
    void stopAllAsyncOperations();

    /// 隐藏所有检测相关Tab（删除图片/ROI后调用）
    void hideAllDetectionTabs();

    /// 统一恢复Tab可见性（图片切换、配置变更后调用）
    /// 优先级：检测项Tab > 步骤Tab
    void restoreTabVisibility();

    /// 加载指定图片的Pipeline配置到PipelineManager（复用模式：loadImagePipelineConfig + setConfig + rebuildPipeline）
    void applyImagePipelineConfig(const QString& imageId);

    Ui::MainWindow *ui;
    ImageView *m_view = nullptr;

    PipelineManager* m_pipelineManager = nullptr;
    RoiManager m_roiManager;
    SystemMonitor* m_systemMonitor = nullptr;
    FileManager* m_fileManager = nullptr;

    bool m_isDestroying = false;

    // Tab管理器
    TabManager* m_tabManager = nullptr;

    // Pipeline执行模式
    enum class PipelineMode { Config, Execute };
    PipelineMode m_pipelineMode = PipelineMode::Config;
    bool m_needRefresh = false;

    // Controller对象
    RoiUiController* m_roiUiController = nullptr;
    DetectionUiController* m_detectionUiController = nullptr;
    ConfigController* m_configController = nullptr;
    RoiDetectionConfigController* m_roiDetectionConfigController = nullptr;

    // 图片列表管理器
    ImageListManager* m_imageListManager = nullptr;

    // 自动检测控制器
    AutoDetectionController* m_autoDetectionController = nullptr;

    // Pipeline结果处理器
    PipelineResultHandler* m_pipelineResultHandler = nullptr;

    // MQTT 边云协同管理器
    MqttManager* m_mqttManager = nullptr;

    // 云平台看板管理器
    CloudDashboardManager* m_cloudDashboardManager = nullptr;

    // 显示模式管理器
    DisplayModeManager* m_displayModeManager = nullptr;

    // 检测方案管理器
    ProfileManager* m_profileManager = nullptr;
    // 方案控制器
    ProfileController* m_profileController = nullptr;

    // Toast 通知浮标
    ToastNotification* m_toast = nullptr;

    // 基准性能显示
    QLabel* m_timingLabel = nullptr;
    QTimer* m_benchmarkUiTimer = nullptr;

    // 动画相关
    QGraphicsOpacityEffect* m_pageOpacityEffect = nullptr;
    QPropertyAnimation* m_pageFadeAnimation = nullptr;
};

#endif // MAINWINDOW_H