#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QFileDialog>
#include <QMessageBox>
#include <QSlider>
#include <QSpinBox>
#include <QListWidgetItem>
#include <QTimer>
#include <QStack>
#include <QSignalBlocker>
#include <QtConcurrent/QtConcurrent>
#include <QFutureWatcher>
#include <QStandardItemModel>

#include <opencv2/opencv.hpp>

#include "image_view.h"
#include "image_utils.h"
#include "halcon_algorithm.h"
#include "pipeline_manager.h"
#include "system_monitor.h"
#include "file_manager.h"
#include "config_manager.h"
#include "widgets/image_tab_widget.h"
#include "widgets/enhance_tab_widget.h"
#include "widgets/filter_tab_widget.h"
#include "widgets/template_tab_widget.h"
#include "widgets/line_tab_widget.h"
#include "widgets/extract_tab_widget.h"
#include "widgets/process_tab_widget.h"
#include "widgets/judge_tab_widget.h"
#include "widgets/barcode_tab_widget.h"
#include "widgets/roi_list_widget.h"
#include "roi_config.h"
#include "detection_manager.h"
#include "log_page.h"
#include "controllers/roi_ui_controller.h"
#include "controllers/detection_ui_controller.h"
#include "controllers/config_controller.h"

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
 * 3. ROI管理
 * 4. 协调各模块工作
 */

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void on_btn_openImg_clicked();
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


private:
    void setupUI();
    void setupConnections();
    void processAndDisplay();
    void showImage(const cv::Mat& img);
    void setDisplayModeForCurrentTab();
    
    // 检测项管理相关
    void setupDetectionManager();
    void setupTreeView();
    void onAddDetectionClicked();
    void onDetectionItemClicked(const QModelIndex& index);
    void onDeleteDetectionClicked();
    void updateTreeView();
    void switchToTabConfig(const TabConfig& config);
    void onRoiDetectionConfigRequested(const QString& roiId);

private:
    Ui::MainWindow *ui;
    ImageView *m_view;

    PipelineManager* m_pipelineManager;
    RoiManager m_roiManager;
    SystemMonitor* m_systemMonitor;
    FileManager* m_fileManager;
    QTimer* m_processDebounceTimer;

    int m_currentTabIndex;
    bool m_isDestroying = false;

    void setupSystemMonitor();

    void saveConfig();
    void loadConfig();
    void collectConfigFromUI(AppConfig& config);
    void applyConfigToUI(const AppConfig& config);

    std::unique_ptr<EnhanceTabWidget> m_enhanceTabWidget;
    std::unique_ptr<FilterTabWidget> m_filterTabWidget;
    std::unique_ptr<TemplateTabWidget> m_templateTabWidget;
    std::unique_ptr<LineDetectTabWidget> m_lineDetectTabWidget;
    std::unique_ptr<ExtractTabWidget> m_extractTabWidget;
    std::unique_ptr<ProcessTabWidget> m_processTabWidget;
    std::unique_ptr<JudgeTabWidget> m_judgeTabWidget;
    std::unique_ptr<BarcodeTabWidget> m_barcodeTabWidget;
    
    std::unique_ptr<ImageTabWidget> m_imageTabWidget;

    // 多线程处理相关
    QFutureWatcher<PipelineContext> m_pipelineWatcher;
    bool m_isProcessing = false;

    // ROI配置管理
    MultiRoiConfig* m_multiRoiConfig;
    
    // Tab Widget列表（用于动态切换）
    QList<QWidget*> m_tabWidgets;
    QStringList m_tabNames;
    
    // Controller对象
    RoiUiController* m_roiUiController;
    DetectionUiController* m_detectionUiController;
    ConfigController* m_configController;

};


#endif // MAINWINDOW_H
