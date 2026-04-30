#pragma once

#include <QWidget>
#include <QTimer>
#include <QThread>
#include "pipeline_manager.h"
#include "algorithm/dnn_inference.h"
#include "widgets/i_signal_connectable.h"
#include "config/vision_inspection_config.h"

namespace Ui {
class ObjectDetectionTabForm;
}

class VideoSourceWidget;

/**
 * @brief 目标检测Tab组件 - 集成视频源 + 预处理 + 检测配置
 *
 * 重构后的目标检测Tab，将视频源选择、图像预处理、DNN检测配置
 * 统一到一个Tab中，提供一站式检测配置体验。
 */
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

    /**
     * 获取视觉检测配置
     */
    VisionInspectionConfig getVisionConfig() const { return m_visionConfig; }
    
    /**
     * 设置视觉检测配置
     */
    void setVisionConfig(const VisionInspectionConfig& config);

    void connectSignals(PipelineManager* pm, RoiManager* rm,
                        ImageView* view, RoiUiController* roiCtrl,
                        std::function<void()> requestRefresh,
                        std::function<void()> processAndDisplay) override;

signals:
    void detectionConfigChanged();
    void modelLoadFinished(bool success, const QString& message);
    void videoSourceChanged(VisionInspectionConfig::VideoSourceType type);

private slots:
    void onApplyClicked();
    void onBrowseModel();
    void onBrowseConfig();
    void onVideoSourceTypeChanged(VisionInspectionConfig::VideoSourceType type);

private:
    void setupConnections();
    void updateConfig();
    void setupVideoSourceTab();

    Ui::ObjectDetectionTabForm* m_ui;
    PipelineManager* m_pipelineManager;
    DnnInference m_dnnInference;
    QString m_currentModelPath;  // 当前已加载的模型路径，用于避免重复加载
    
    // 视觉检测配置
    VisionInspectionConfig m_visionConfig;
    VideoSourceWidget* m_videoSourceWidget;  // 视频源组件
};
