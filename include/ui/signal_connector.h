#pragma once

#include <QObject>
#include <QString>

class MainWindow;
class PipelineManager;
class RoiManager;
class ImageView;
class TabManager;
class RoiUiController;
class DetectionUiController;

/**
 * 信号连接器 - 负责建立各模块之间的信号连接
 * 
 * 职责：
 * 1. 建立 Tab Widget 与 MainWindow 之间的信号连接
 * 2. 建立 Controller 与 MainWindow 之间的信号连接
 * 3. 集中管理所有跨模块的信号路由
 * 
 * 从 MainWindow 中提取，降低 MainWindow 的职责复杂度。
 */
class SignalConnector : public QObject
{
    Q_OBJECT

public:
    explicit SignalConnector(
        MainWindow* mainWindow,
        PipelineManager* pipelineManager,
        RoiManager* roiManager,
        ImageView* view,
        TabManager* tabManager,
        RoiUiController* roiUiController,
        DetectionUiController* detectionUiController,
        QObject* parent = nullptr
    );

    /// 建立 Tab Widget 的信号连接（由 TabManager::tabCreated 信号触发）
    void connectTabSignals(const QString& tabName, QWidget* widget);

private:
    MainWindow* m_mainWindow;
    PipelineManager* m_pipelineManager;
    RoiManager* m_roiManager;
    ImageView* m_view;
    TabManager* m_tabManager;
    RoiUiController* m_roiUiController;
    DetectionUiController* m_detectionUiController;

    // 各 Tab 的独立连接方法
    void connectImageTab(QWidget* widget);
    void connectVideoTab(QWidget* widget);
    void connectEnhanceTab(QWidget* widget);
    void connectFilterTab(QWidget* widget);
    void connectTemplateTab(QWidget* widget);
    void connectProcessTab(QWidget* widget);
    void connectExtractTab(QWidget* widget);
    void connectJudgeTab(QWidget* widget);
    void connectLineTab(QWidget* widget);
    void connectObjectDetectionTab(QWidget* widget);
};