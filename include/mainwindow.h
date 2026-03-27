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

#include <opencv2/opencv.hpp>

#include "image_view.h"
#include "image_utils.h"
#include "halcon_algorithm.h"
#include "pipeline_manager.h"
#include "system_monitor.h"
#include "file_manager.h"
#include "config_manager.h"
#include "log_page.h"
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
#include "widgets/detection_config_widget.h"
#include "roi_config.h"

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
    void onRoiSelected(const QRectF &roiRect);
    void on_btn_clearLog_clicked();
    void on_btn_openLog_clicked();
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

private:
    Ui::MainWindow *ui;
    ImageView *m_view;

    PipelineManager* m_pipelineManager;
    RoiManager m_roiManager;
    SystemMonitor* m_systemMonitor;
    FileManager* m_fileManager;
    QTimer* m_processDebounceTimer;

    int m_currentTabIndex;
    bool m_needsReprocess = false;
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

    
    std::unique_ptr<LogPage> m_logPage;
    std::unique_ptr<ImageTabWidget> m_imageTabWidget;

    // 多线程处理相关
    QFutureWatcher<PipelineContext> m_pipelineWatcher;
    bool m_isProcessing = false;

    // ========== 多ROI管理相关 ==========
    
    // ROI配置数据
    MultiRoiConfig m_multiRoiConfig;
    
    // UI组件
    std::unique_ptr<RoiListWidget> m_roiListWidget;
    std::unique_ptr<DetectionConfigWidget> m_detectionConfigWidget;
    
    // ========== 多ROI管理方法 ==========
    
    /**
     * @brief 初始化多ROI管理组件
     */
    void initMultiRoiManagement();
    
    /**
     * @brief 连接多ROI管理信号
     */
    void connectMultiRoiSignals();
    
    /**
     * @brief 更新图像显示（支持多ROI）
     */
    void updateImageWithMultipleRois();
    
    /**
     * @brief 添加新ROI
     * @param roiName ROI名称
     */
    void addNewRoi(const QString& roiName);
    
    /**
     * @brief 删除ROI
     * @param roiId ROI ID
     */
    void deleteRoi(const QString& roiId);
    
    /**
     * @brief 选中ROI
     * @param roiId ROI ID
     */
    void selectRoi(const QString& roiId);
    
    /**
     * @brief 重命名ROI
     * @param roiId ROI ID
     * @param newName 新名称
     */
    void renameRoi(const QString& roiId, const QString& newName);
    
    /**
     * @brief 切换ROI激活状态
     * @param roiId ROI ID
     * @param active 激活状态
     */
    void toggleRoiActive(const QString& roiId, bool active);
    
    /**
     * @brief 添加检测项
     * @param type 检测类型
     * @param name 检测项名称
     */
    void addDetectionItem(DetectionType type, const QString& name);
    
    /**
     * @brief 删除检测项
     * @param itemId 检测项ID
     */
    void deleteDetectionItem(const QString& itemId);
    
    /**
     * @brief 更新检测项配置
     * @param itemId 检测项ID
     */
    void updateDetectionItem(const QString& itemId);
    
    /**
     * @brief 保存多ROI配置
     * @param filePath 文件路径
     */
    void saveMultiRoiConfig(const QString& filePath);
    
    /**
     * @brief 加载多ROI配置
     * @param filePath 文件路径
     */
    void loadMultiRoiConfig(const QString& filePath);

};


#endif // MAINWINDOW_H
