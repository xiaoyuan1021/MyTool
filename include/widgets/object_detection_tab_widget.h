#pragma once

#include <QWidget>
#include <QTimer>
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

    void updateDetectionResults(const cv::Mat& image, const std::vector<cv::Rect>& boxes, 
                                const std::vector<float>& confidences, const std::vector<int>& classIds,
                                const std::vector<std::string>& classNames);

signals:
    void detectionConfigChanged();

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
