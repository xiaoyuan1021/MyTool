#pragma once

#include <QObject>
#include <QString>

#include <opencv2/opencv.hpp>

class PipelineManager;
class RoiManager;
class ImageView;
class RoiUiController;
class ISignalConnectable;

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
        PipelineManager* pipelineManager,
        RoiManager* roiManager,
        ImageView* view,
        RoiUiController* roiUiController,
        QObject* parent = nullptr
    );

signals:
    void requestRefresh();
    void processAndDisplay();
    void showImage(const cv::Mat& img);

public:
    /// 建立 Tab Widget 的信号连接（由 TabManager::tabCreated 信号触发）
    /// 使用ISignalConnectable接口，不再依赖具体的Tab类型
    void connectTabSignals(const QString& tabName, QWidget* widget);

private:
    PipelineManager* m_pipelineManager;
    RoiManager* m_roiManager;
    ImageView* m_view;
    RoiUiController* m_roiUiController;
};