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

#include <opencv2/opencv.hpp>

#include "image_view.h"
#include "image_utils.h"
#include "halcon_algorithm.h"
#include "pipeline_manager.h"  // ✅ 新增：使用Pipeline管理器
#include "template_controller.h"
#include "system_monitor.h"
#include "file_manager.h"
#include "controllers/image_tab_controller.h"
#include "controllers/enhancement_tab_controller.h"

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
    // ========== 文件操作 ==========
    void on_btn_openImg_clicked();
    void on_btn_saveImg_clicked();
    
    // ========== ROI操作 ==========
    void on_btn_drawRoi_clicked();
    void on_btn_resetROI_clicked();
    void onRoiSelected(const QRectF &roiRect);

    // ========== 算法队列操作 ==========
    void on_btn_addOption_clicked();
    void on_btn_removeOption_clicked();

    // ========== 区域筛选 ==========
    void on_btn_select_clicked();

    // ========== 其他 ==========
    void on_comboBox_channels_currentIndexChanged(int index);

    void on_comboBox_select_currentIndexChanged(int index);

    void on_comboBox_condition_currentIndexChanged(int index);

    void on_btn_clearFilter_clicked();

    void on_btn_addFilter_clicked();

    void on_btn_optionUp_clicked();

    void on_btn_optionDown_clicked();

    void onAlgorithmTypeChanged(int index);

    void on_btn_runTest_clicked();

    void on_btn_clearLog_clicked();

    void on_btn_drawRegion_clicked();

    void on_btn_clearRegion_clicked();

    void onAlgorithmSelectionChanged(QListWidgetItem* current,QListWidgetItem* previous);

    void on_comboBox_filterMode_currentIndexChanged(int index);

    void on_btn_openLog_clicked();

    void filterColorChannelsChanged();

    void on_tabWidget_currentChanged(int index);


private:
    // ========== 初始化 ==========
    void setupUI();
    void setupConnections();
    void setupSliderSpinBoxPair(QSlider* slider, QSpinBox* spinbox,
                                int min, int max, int defaultValue);

    // ========== 核心处理 ==========
    void processAndDisplay();  // ✅ 统一的处理入口
    void showImage(const cv::Mat& img);

    void saveCurrentEdit();
    void loadAlgorithmParameters(int index);

private:
    // UI
    Ui::MainWindow *ui;
    ImageView *m_view;

    // ✅ 核心模块（简化为两个）
    PipelineManager* m_pipelineManager;  // Pipeline管理器
    RoiManager m_roiManager;              // ROI管理器
    SystemMonitor * m_systemMonitor;
    FileManager* m_fileManager;           // ✅ 文件管理器

    // ✅ 新增：防抖定时器
    QTimer* m_processDebounceTimer;

    int m_currentTabIndex;  // 记录当前tab索引

    bool m_needsReprocess = false;

    void setupSystemMonitor();

    // ✅ 新增：存储绘制的多边形点
    QVector <QPointF> m_drawnpoints;
    // ✅ 新增：绘制模式标志
    bool m_isDrawingRegion;
    // ✅ 新增：显示多边形的图形项
    QGraphicsPolygonItem * m_polygonItem;
    void calculateRegionFeatures(const QVector<QPointF>& points);
    int m_editingAlgorithmIndex=-1;

    std::unique_ptr<ImageTabController> m_imageTabController;
    std::unique_ptr<EnhancementTabController> m_enhancementController;
    std::unique_ptr<TemplateController> m_templateController;

};

#endif // MAINWINDOW_H
