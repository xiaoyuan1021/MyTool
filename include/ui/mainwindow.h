#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QTimer>
#include <QFutureWatcher>

#include <opencv2/opencv.hpp>

#include "pipeline_manager.h"
#include "roi_manager.h"

// 前向声明
class ImageView;
class SignalConnector;
class SystemMonitor;
class FileManager;
class TabManager;
class ImageListManager;
class RoiUiController;
class DetectionUiController;
class ConfigController;
class AutoDetectionController;
class PipelineResultHandler;

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

private slots:
    void on_btn_openImg_clicked();
    void on_btn_openVideo_clicked();
    void on_btn_saveImg_clicked();
    void on_btn_drawRoi_clicked();
    void on_btn_resetROI_clicked();
    void on_btn_addROI_clicked();
    void on_btn_deleteROI_clicked();
    void on_btn_switchROI_clicked();
    void onRoiSelected(const QRectF &roiRect);
    void on_tabWidget_currentChanged(int index);
    void on_btn_Log_clicked();
    void on_btn_Home_clicked();
    void on_btn_saveConfig_clicked();
    void on_btn_importConfig_clicked();
    void on_btn_startAutoDetection_clicked();
    void on_btn_stopAutoDetection_clicked();

private:
    // 初始化方法
    void setupBasicInfrastructure();
    void setupManagers();
    void setupControllers();
    void setupResultHandler();
    void setupUI();
    void setupConnections();

    void setDisplayModeForCurrentTab();
    void ensureTabExists(const QString& tabName);

    Ui::MainWindow *ui;
    ImageView *m_view = nullptr;

    PipelineManager* m_pipelineManager = nullptr;
    RoiManager m_roiManager;
    SystemMonitor* m_systemMonitor = nullptr;
    FileManager* m_fileManager = nullptr;
    QTimer* m_processDebounceTimer = nullptr;

    bool m_isDestroying = false;

    // Tab管理器
    TabManager* m_tabManager = nullptr;

    // 多线程处理相关
    QFutureWatcher<PipelineContext> m_pipelineWatcher;
    bool m_isProcessing = false;
    bool m_hasPendingProcess = false;

    // Controller对象
    RoiUiController* m_roiUiController = nullptr;
    DetectionUiController* m_detectionUiController = nullptr;
    ConfigController* m_configController = nullptr;

    // 图片列表管理器
    ImageListManager* m_imageListManager = nullptr;

    // 自动检测控制器
    AutoDetectionController* m_autoDetectionController = nullptr;

    // 信号连接器
    SignalConnector* m_signalConnector = nullptr;

    // Pipeline结果处理器
    PipelineResultHandler* m_pipelineResultHandler = nullptr;
};

#endif // MAINWINDOW_H