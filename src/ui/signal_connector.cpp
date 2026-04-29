#include "signal_connector.h"
#include "logger.h"
#include "ui/image_view.h"
#include "controllers/roi_ui_controller.h"

// Tab Widget 头文件
#include "widgets/image_tab_widget.h"
#include "widgets/video_tab_widget.h"
#include "widgets/enhance_tab_widget.h"
#include "widgets/filter_tab_widget.h"
#include "widgets/template_tab_widget.h"
#include "widgets/process_tab_widget.h"
#include "widgets/extract_tab_widget.h"
#include "widgets/judge_tab_widget.h"
#include "widgets/line_tab_widget.h"
#include "widgets/object_detection_tab_widget.h"
#include "widgets/barcode_tab_widget.h"

#include <QShortcut>

SignalConnector::SignalConnector(
    PipelineManager* pipelineManager,
    RoiManager* roiManager,
    ImageView* view,
    RoiUiController* roiUiController,
    QObject* parent)
    : QObject(parent)
    , m_pipelineManager(pipelineManager)
    , m_roiManager(roiManager)
    , m_view(view)
    , m_roiUiController(roiUiController)
{
}

void SignalConnector::connectTabSignals(const QString& tabName, QWidget* widget)
{
    if (tabName == "图像") {
        connectImageTab(widget);
    }
    else if (tabName == "视频") {
        connectVideoTab(widget);
    }
    else if (tabName == "增强") {
        connectEnhanceTab(widget);
    }
    else if (tabName == "过滤") {
        connectFilterTab(widget);
    }
    else if (tabName == "补正") {
        connectTemplateTab(widget);
    }
    else if (tabName == "处理") {
        connectProcessTab(widget);
    }
    else if (tabName == "提取") {
        connectExtractTab(widget);
    }
    else if (tabName == "判定") {
        connectJudgeTab(widget);
    }
    else if (tabName == "直线") {
        connectLineTab(widget);
    }
    else if (tabName == "目标检测") {
        connectObjectDetectionTab(widget);
    }
    else if (tabName == "条码") {
        connectBarcodeTab(widget);
    }
}

void SignalConnector::connectImageTab(QWidget* widget)
{
    auto* tab = qobject_cast<ImageTabWidget*>(widget);
    connect(tab, &ImageTabWidget::channelChanged,
            this, [this](int channel) {
                PipelineConfig cfg = m_pipelineManager->getConfigSnapshot();
                cfg.channel = static_cast<PipelineConfig::Channel>(channel);
                m_pipelineManager->setConfig(cfg);
                emit requestRefresh();
            });
}

void SignalConnector::connectVideoTab(QWidget* widget)
{
    auto* tab = qobject_cast<VideoTabWidget*>(widget);
    connect(tab, &VideoTabWidget::videoFrameReady,
            this, [this](const cv::Mat& frame) {
                if (!frame.empty()) {
                    m_roiManager->setFullImage(frame);
                    m_view->clearRoi();
                    emit processAndDisplay();
                }
            });
}

void SignalConnector::connectEnhanceTab(QWidget* widget)
{
    auto* tab = qobject_cast<EnhanceTabWidget*>(widget);
    connect(tab, &EnhanceTabWidget::processRequested,
            this, [this]() { emit requestRefresh(); });
    // 增强参数变化时，实时回写到当前ROI，防止多ROI间亮度/对比度等参数串扰
    connect(tab, &EnhanceTabWidget::enhanceConfigChanged,
            this, [this]() {
                m_roiUiController->saveCurrentRoiPipelineConfig();
            });
}

void SignalConnector::connectFilterTab(QWidget* widget)
{
    auto* tab = qobject_cast<FilterTabWidget*>(widget);
    connect(tab, &FilterTabWidget::filterConfigChanged,
            this, [this]() { emit requestRefresh(); });
}

void SignalConnector::connectTemplateTab(QWidget* widget)
{
    auto* tab = qobject_cast<TemplateTabWidget*>(widget);
    connect(tab, &TemplateTabWidget::imageToShow,
            this, [this](const cv::Mat& img) { emit showImage(img); });
    connect(tab, &TemplateTabWidget::requestShowImage,
            this, [this](const cv::Mat& img) { emit showImage(img); });
    connect(tab, &TemplateTabWidget::templateCreated,
            this, [](const QString& n) {
                Logger::instance()->info(QString("模板已创建: %1").arg(n));
            });
    connect(tab, &TemplateTabWidget::matchCompleted,
            this, [](int count) {
                Logger::instance()->info(
                    QString("匹配完成，找到 %1 个目标").arg(count));
            });
    QShortcut* sc = new QShortcut(Qt::Key_Escape, this);
    connect(sc, &QShortcut::activated, tab,
            &TemplateTabWidget::clearMatchResults);
}

void SignalConnector::connectProcessTab(QWidget* widget)
{
    auto* tab = qobject_cast<ProcessTabWidget*>(widget);
    connect(tab, &ProcessTabWidget::algorithmChanged,
            this, [this]() { emit requestRefresh(); });
}

void SignalConnector::connectExtractTab(QWidget* widget)
{
    auto* tab = qobject_cast<ExtractTabWidget*>(widget);
    connect(tab, &ExtractTabWidget::extractionChanged,
            this, [this]() { emit requestRefresh(); });
}

void SignalConnector::connectJudgeTab(QWidget* widget)
{
    auto* tab = qobject_cast<JudgeTabWidget*>(widget);
    connect(tab, &JudgeTabWidget::judgeConfigChanged,
            m_roiUiController, &RoiUiController::updateBlobDetectionConfig);
}

void SignalConnector::connectLineTab(QWidget* widget)
{
    auto* tab = qobject_cast<LineDetectTabWidget*>(widget);
    connect(tab, &LineDetectTabWidget::requestDrawReferenceLine,
            this, [this]() { m_view->startReferenceLineDrawing(); });
    connect(tab, &LineDetectTabWidget::requestClearReferenceLine,
            this, [this]() {
                m_view->clearReferenceLine();
                emit processAndDisplay();
            });
    connect(m_view, &ImageView::referenceLineDrawn,
            tab, &LineDetectTabWidget::setReferenceLine);
}

void SignalConnector::connectObjectDetectionTab(QWidget* widget)
{
    auto* tab = qobject_cast<ObjectDetectionTabWidget*>(widget);
    connect(tab, &ObjectDetectionTabWidget::detectionConfigChanged,
            this, [this]() {
                emit requestRefresh();
            });
}

void SignalConnector::connectBarcodeTab(QWidget* widget)
{
    auto* tab = qobject_cast<BarcodeTabWidget*>(widget);
    connect(tab, &BarcodeTabWidget::requestApplyBarcodeSettings,
            this, [this]() {
                emit requestRefresh();
            });
}