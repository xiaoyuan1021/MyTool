#pragma once

#include <QWidget>
#include <QTimer>
#include <QThread>
#include "pipeline_manager.h"
#include "algorithm/dnn_inference.h"

namespace Ui {
class ObjectDetectionTabForm;
}

class ObjectDetectionTabWidget : public QWidget {
    Q_OBJECT

public:
    explicit ObjectDetectionTabWidget(PipelineManager* pipelineManager, QWidget* parent = nullptr);
    ~ObjectDetectionTabWidget();

    /**
     * 对当前图像执行目标检测
     * @param image 输入图像
     * @return 检测结果列表
     */
    std::vector<DetectionResult> runDetection(const cv::Mat& image);

    /**
     * 判断模型是否已加载
     */
    bool isModelLoaded() const;

    /**
     * 获取检测参数
     */
    float getConfidenceThreshold() const;
    float getNmsThreshold() const;
    int getInputWidth() const;
    int getInputHeight() const;

    /**
     * 更新检测结果显示
     */
    void updateDetectionResults(const std::vector<DetectionResult>& results);

signals:
    void detectionConfigChanged();
    void modelLoadFinished(bool success, const QString& message);

private slots:
    void onApplyClicked();
    void onBrowseModel();
    void onBrowseConfig();

private:
    void setupConnections();
    void updateConfig();

    Ui::ObjectDetectionTabForm* m_ui;
    PipelineManager* m_pipelineManager;
    DnnInference m_dnnInference;
};
