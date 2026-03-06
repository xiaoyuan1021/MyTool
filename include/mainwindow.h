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
#include "controllers/filter_tab_controller.h"
#include "controllers/algorithm_tab_controller.h"
#include "controllers/extract_tab_controller.h"

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

    // ========== 算法队列操作现在由AlgorithmTabController处理 ==========

    // ========== 提取相关操作现在由ExtractTabController处理 ==========

    // ========== 算法相关操作现在由AlgorithmTabController处理 ==========

    void on_btn_runTest_clicked();

    void on_btn_clearLog_clicked();

    // ========== 区域绘制现在由ExtractTabController处理 ==========

    // ========== 算法选择现在由AlgorithmTabController处理 ==========

    // ========== 过滤模式现在由FilterTabController处理 ==========

    void on_btn_openLog_clicked();

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
    void setDisplayModeForCurrentTab();

    // ========== 算法参数处理现在由AlgorithmTabController处理 ==========

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
    // ========== 区域特征计算现在由ExtractTabController处理 ==========

    // ========== 算法编辑索引现在由AlgorithmTabController管理 ==========

    std::unique_ptr<ImageTabController> m_imageTabController;
    std::unique_ptr<EnhancementTabController> m_enhancementController;
    std::unique_ptr<TemplateController> m_templateController;
    std::unique_ptr<FilterTabController> m_filterController;
    std::unique_ptr<AlgorithmTabController> m_algorithmController;
    std::unique_ptr<ExtractTabController> m_extractController;

};

#endif // MAINWINDOW_H
