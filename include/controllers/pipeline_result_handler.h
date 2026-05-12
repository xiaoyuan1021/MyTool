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
    
    void handleJudgeTabResult(const PipelineContext& result);
    void handleLineTabResult(const PipelineContext& result);
    void handleBarcodeTabResult(const PipelineContext& result);
    void handleObjectDetection(cv::Mat& displayImage);
    void drawDetectionResults(cv::Mat& image, const std::vector<DetectionResult>& results);
};

#endif // PIPELINE_RESULT_HANDLER_H