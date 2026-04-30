#pragma once

#include <QWidget>
#include <QTimer>
#include <QThread>
#include "pipeline_manager.h"
#include "algorithm/dnn_inference.h"
#include "algorithm/ort_inference.h"
#include "widgets/i_signal_connectable.h"

namespace Ui {
class ObjectDetectionTabForm;
}
class ObjectDetectionTabWidget : public QWidget, public ISignalConnectable {
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

    void connectSignals(PipelineManager* pm, RoiManager* rm,
                        ImageView* view, RoiUiController* roiCtrl,
                        std::function<void()> requestRefresh,
                        std::function<void()> processAndDisplay) override;

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
    DnnInference m_dnnInference;   // OpenCV DNN (CPU 回退)
    OrtInference m_ortInference;   // ONNX Runtime (GPU 加速)
    bool m_useOrt = true;          // 是否优先使用 ONNX Runtime
    QString m_currentModelPath;    // 当前已加载的模型路径，用于避免重复加载
};
