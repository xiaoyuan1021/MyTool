#include "signal_connector.h"
#include "mainwindow.h"
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

#include <QShortcut>

SignalConnector::SignalConnector(
    MainWindow* mainWindow,
    PipelineManager* pipelineManager,
    RoiManager* roiManager,
    ImageView* view,
    TabManager* tabManager,
    RoiUiController* roiUiController,
    DetectionUiController* detectionUiController,
    QObject* parent)
    : QObject(parent)
    , m_mainWindow(mainWindow)
    , m_pipelineManager(pipelineManager)
    , m_roiManager(roiManager)
    , m_view(view)
    , m_tabManager(tabManager)
    , m_roiUiController(roiUiController)
    , m_detectionUiController(detectionUiController)
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
}

void SignalConnector::connectImageTab(QWidget* widget)
{
    auto* tab = qobject_cast<ImageTabWidget*>(widget);
    connect(tab, &ImageTabWidget::channelChanged,
            m_mainWindow, [this](int channel) {
                PipelineConfig cfg = m_pipelineManager->getConfigSnapshot();
                cfg.channel = static_cast<PipelineConfig::Channel>(channel);
                m_pipelineManager->setConfig(cfg);
                m_mainWindow->processAndDisplay();
            });
}

void SignalConnector::connectVideoTab(QWidget* widget)
{
    auto* tab = qobject_cast<VideoTabWidget*>(widget);
    connect(tab, &VideoTabWidget::videoFrameReady,
            m_mainWindow, [this](const cv::Mat& frame) {
                if (!frame.empty()) {
                    m_roiManager->setFullImage(frame);
                    m_view->clearRoi();
                    m_mainWindow->processAndDisplay();
                }
            });
}

void SignalConnector::connectEnhanceTab(QWidget* widget)
{
    auto* tab = qobject_cast<EnhanceTabWidget*>(widget);
    connect(tab, &EnhanceTabWidget::processRequested,
            m_mainWindow, [this]() { m_mainWindow->processAndDisplay(); });
    // 增强参数变化时，实时回写到当前ROI，防止多ROI间亮度/对比度等参数串扰
    connect(tab, &EnhanceTabWidget::enhanceConfigChanged,
            m_mainWindow, [this]() {
                m_roiUiController->saveCurrentRoiPipelineConfig();
            });
}

void SignalConnector::connectFilterTab(QWidget* widget)
{
    auto* tab = qobject_cast<FilterTabWidget*>(widget);
    connect(tab, &FilterTabWidget::filterConfigChanged,
            m_mainWindow, [this]() { m_mainWindow->processAndDisplay(); });
}

void SignalConnector::connectTemplateTab(QWidget* widget)
{
    auto* tab = qobject_cast<TemplateTabWidget*>(widget);
    connect(tab, &TemplateTabWidget::imageToShow,
            m_mainWindow, [this](const cv::Mat& img) { m_mainWindow->showImage(img); });
    connect(tab, &TemplateTabWidget::templateCreated,
            m_mainWindow, [](const QString& n) {
                Logger::instance()->info(QString("模板已创建: %1").arg(n));
            });
    connect(tab, &TemplateTabWidget::matchCompleted,
            m_mainWindow, [](int count) {
                Logger::instance()->info(
                    QString("匹配完成，找到 %1 个目标").arg(count));
            });
    QShortcut* sc = new QShortcut(Qt::Key_Escape, m_mainWindow);
    connect(sc, &QShortcut::activated, tab,
            &TemplateTabWidget::clearMatchResults);
}

void SignalConnector::connectProcessTab(QWidget* widget)
{
    auto* tab = qobject_cast<ProcessTabWidget*>(widget);
    connect(tab, &ProcessTabWidget::algorithmChanged,
            m_mainWindow, [this]() { m_mainWindow->processAndDisplay(); });
}

void SignalConnector::connectExtractTab(QWidget* widget)
{
    auto* tab = qobject_cast<ExtractTabWidget*>(widget);
    connect(tab, &ExtractTabWidget::extractionChanged,
            m_mainWindow, [this]() { m_mainWindow->processAndDisplay(); });
}

void SignalConnector::connectJudgeTab(QWidget* widget)
{
    auto* tab = qobject_cast<JudgeTabWidget*>(widget);
    // 【Bug1修复补充】当判定Tab的min/max值变化时，同步写回当前选中ROI的DetectionItem.config
    connect(tab, &JudgeTabWidget::judgeConfigChanged,
            m_mainWindow, [this](int minCount, int maxCount) {
                QString roiId = m_roiUiController->getCurrentSelectedRoiId();
                RoiConfig* roi = m_roiManager->getRoiConfig(roiId);
                if (!roi) return;
                // 找到当前选中的Blob检测项，更新其config中的minBlobCount/maxBlobCount
                for (auto& detItem : roi->detectionItems) {
                    if (detItem.type == DetectionType::Blob && detItem.enabled) {
                        BlobAnalysisConfig blobConfig;
                        blobConfig.fromJson(detItem.config);
                        blobConfig.minBlobCount = minCount;
                        blobConfig.maxBlobCount = maxCount;
                        detItem.config = blobConfig.toJson();
                        Logger::instance()->info(QString("[SignalConnector] 判定Tab值变化，同步到DetectionItem.config: min=%1, max=%2")
                            .arg(minCount).arg(maxCount));
                        break;
                    }
                }
            });
}

void SignalConnector::connectLineTab(QWidget* widget)
{
    auto* tab = qobject_cast<LineDetectTabWidget*>(widget);
    connect(tab, &LineDetectTabWidget::requestDrawReferenceLine,
            m_mainWindow, [this]() { m_view->startReferenceLineDrawing(); });
    connect(tab, &LineDetectTabWidget::requestClearReferenceLine,
            m_mainWindow, [this]() {
                m_view->clearReferenceLine();
                m_mainWindow->processAndDisplay();
            });
    connect(m_view, &ImageView::referenceLineDrawn,
            tab, &LineDetectTabWidget::setReferenceLine);
}

void SignalConnector::connectObjectDetectionTab(QWidget* widget)
{
    auto* tab = qobject_cast<ObjectDetectionTabWidget*>(widget);
    connect(tab, &ObjectDetectionTabWidget::detectionConfigChanged,
            m_mainWindow, [this]() {
                // 用户点击"应用"按钮加载模型成功后，触发处理
                // pipeline 完成后会自动检查模型是否已加载并执行目标检测
                m_mainWindow->processAndDisplay();
            });
}
