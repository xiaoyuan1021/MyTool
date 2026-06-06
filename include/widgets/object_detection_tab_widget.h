#pragma once

#include <QWidget>
#include <QTimer>
#include <QThread>
#include "core/i_pipeline_access.h"
#include "algorithm/dnn_inference.h"
#include "algorithm/ort_inference.h"
#include "widgets/i_tab_interfaces.h"

namespace Ui {
class ObjectDetectionTabForm;
}
class ObjectDetectionTabWidget : public QWidget, public ISignalConnectable {
    Q_OBJECT

public:
    explicit ObjectDetectionTabWidget(IPipelineAccess* pipelineAccess, QWidget* parent = nullptr);
    ~ObjectDetectionTabWidget();

    /**
     * 目标检测（使用 OpenCV DNN，用于标签页静图检测）
     * @param image 输入图像
     * @return 检测结果列表
     */
    std::vector<DetectionResult> runDetection(const cv::Mat& image);

    /**
     * 目标检测（使用 ONNX Runtime，用于视频推理）
     * @param image 输入图像
     * @return 检测结果列表
     */
    std::vector<DetectionResult> runDetectionOrt(const cv::Mat& image);

    /**
     * 判断模型是否已加载
     */
    bool isModelLoaded() const;

    /**
     * 判断 ONNX Runtime 推理后端是否就绪
     */
    bool isOrtLoaded() const;

    /**
     * 获取检测参数
     */
    float getConfidenceThreshold() const;
    float getNmsThreshold() const;

    /**
     * [NOTE] 获取当前配置（用于同步到DetectionItem.config）
     */
    ObjectDetectionConfig getDetectionConfig() const;

    /**
     * [NOTE] 设置检测配置（从DetectionItem.config恢复到UI）
     */
    void setDetectionConfig(const ObjectDetectionConfig& config);

    /**
     * 更新检测结果显示
     */
    void updateDetectionResults(const std::vector<DetectionResult>& results);

    void connectSignals(const SignalContext& ctx,
                        std::function<void()> onExecutePipeline,
                        std::function<void()> onConfigSaved = nullptr) override;

signals:
    void detectionConfigChanged();
    void modelLoadFinished(bool success, const QString& message);

private slots:
    void onApplyClicked();
    void onBrowseModel();
    void onBrowseConfig();
    void autoLoadDefaultModel();

private:
    void setupConnections();
    void updateConfig();

    Ui::ObjectDetectionTabForm* m_ui;
    IPipelineAccess* m_pipeline;
    DnnInference m_dnnInference;   // OpenCV DNN（目标检测标签页使用）
    OrtInference m_ortInference;   // ONNX Runtime（视频推理使用）
    QString m_currentModelPath;    // 当前已加载的模型路径，用于避免重复加载
};
