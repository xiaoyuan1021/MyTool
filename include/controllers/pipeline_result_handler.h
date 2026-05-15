#ifndef PIPELINE_RESULT_HANDLER_H
#define PIPELINE_RESULT_HANDLER_H

#include <QObject>
#include <QFutureWatcher>
#include "pipeline.h"
#include "pipeline_manager.h"
#include "widgets/tab_manager.h"
#include "roi_manager.h"
#include "image_view.h"
#include "data/detection_result_report.h"
#include "algorithm/dnn_inference.h"

class PipelineResultHandler : public QObject
{
    Q_OBJECT
public:
    explicit PipelineResultHandler(QObject *parent = nullptr);
    
    void setDependencies(TabManager* tabManager, RoiManager* roiManager, ImageView* imageView, PipelineManager* pipelineManager);
    void watchPipeline(QFutureWatcher<PipelineContext>* watcher);

    /// 设置视频处理模式（视频帧使用 ONNX Runtime 推理，静图使用 OpenCV DNN）
    void setVideoMode(bool active);

signals:
    void processingFinished();
    void statusMessage(const QString& message, int timeout = 0);

private slots:
    void onPipelineFinished();

private:
    TabManager* m_tabManager = nullptr;
    RoiManager* m_roiManager = nullptr;
    ImageView* m_imageView = nullptr;
    PipelineManager* m_pipelineManager = nullptr;
    QFutureWatcher<PipelineContext>* m_watcher = nullptr;
    
    /// 通过 IResultUpdatable 接口分发结果（不再硬编码 Tab 名称）
    void distributeResults(const PipelineContext& result);
    
    /// 目标检测特殊处理（不走 Pipeline，临时运行推理）
    void handleObjectDetection(cv::Mat& displayImage);
    void drawDetectionResults(cv::Mat& image, const std::vector<DetectionResult>& results);

    bool m_isVideoMode = false;  // 当前是否为视频处理模式
};

#endif // PIPELINE_RESULT_HANDLER_H