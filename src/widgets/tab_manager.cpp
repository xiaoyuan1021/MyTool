#include "widgets/tab_manager.h"
#include "pipeline_manager.h"
#include "roi_manager.h"
#include "image_view.h"
#include "logger.h"

#include "widgets/image_tab_widget.h"
#include "widgets/enhance_tab_widget.h"
#include "widgets/filter_tab_widget.h"
#include "widgets/template_tab_widget.h"
#include "widgets/line_tab_widget.h"
#include "widgets/extract_tab_widget.h"
#include "widgets/process_tab_widget.h"
#include "widgets/judge_tab_widget.h"
#include "widgets/barcode_tab_widget.h"
#include "widgets/video_tab_widget.h"
#include "widgets/object_detection_tab_widget.h"

#include <QShortcut>

TabManager::TabManager(
    QTabWidget* tabWidget,
    PipelineManager* pipelineManager,
    RoiManager* roiManager,
    ImageView* view,
    QTimer* debounceTimer,
    QObject* parent)
    : QObject(parent)
    , m_tabWidget(tabWidget)
    , m_pipelineManager(pipelineManager)
    , m_roiManager(roiManager)
    , m_view(view)
    , m_debounceTimer(debounceTimer)
{
}

TabManager::~TabManager() = default;

void TabManager::ensureTab(const QString& tabName)
{
    if (isCreated(tabName)) return;

    QWidget* widget = createTab(tabName);
    if (!widget) return;

    m_tabWidget->addTab(widget, tabName);
    m_tabs[tabName] = widget;
    m_tabOrder.append(tabName);

    emit tabCreated(tabName, widget);

    Logger::instance()->info(QString("按需创建Tab: %1").arg(tabName));
}

bool TabManager::isCreated(const QString& tabName) const
{
    return m_tabs.contains(tabName);
}

QWidget* TabManager::getTab(const QString& name) const
{
    auto it = m_tabs.find(name);
    return (it != m_tabs.end()) ? it.value() : nullptr;
}

// ========== 类型安全的 getter ==========

EnhanceTabWidget* TabManager::getEnhanceTab() const {
    return dynamic_cast<EnhanceTabWidget*>(getTab("增强"));
}

FilterTabWidget* TabManager::getFilterTab() const {
    return dynamic_cast<FilterTabWidget*>(getTab("过滤"));
}

ExtractTabWidget* TabManager::getExtractTab() const {
    return dynamic_cast<ExtractTabWidget*>(getTab("提取"));
}

JudgeTabWidget* TabManager::getJudgeTab() const {
    return dynamic_cast<JudgeTabWidget*>(getTab("判定"));
}

ProcessTabWidget* TabManager::getProcessTab() const {
    return dynamic_cast<ProcessTabWidget*>(getTab("处理"));
}

LineDetectTabWidget* TabManager::getLineDetectTab() const {
    return dynamic_cast<LineDetectTabWidget*>(getTab("直线"));
}

BarcodeTabWidget* TabManager::getBarcodeTab() const {
    return dynamic_cast<BarcodeTabWidget*>(getTab("条码"));
}

ObjectDetectionTabWidget* TabManager::getObjectDetectionTab() const {
    return dynamic_cast<ObjectDetectionTabWidget*>(getTab("目标检测"));
}

TemplateTabWidget* TabManager::getTemplateTab() const {
    return dynamic_cast<TemplateTabWidget*>(getTab("补正"));
}

ImageTabWidget* TabManager::getImageTab() const {
    return dynamic_cast<ImageTabWidget*>(getTab("图像"));
}

VideoTabWidget* TabManager::getVideoTab() const {
    return dynamic_cast<VideoTabWidget*>(getTab("视频"));
}

// ========== Tab创建 ==========

QWidget* TabManager::createTab(const QString& name)
{
    // 注意：创建时只做对象构造，信号连接由 MainWindow 通过 tabCreated 信号处理
    // 特殊情况：需要额外初始化的 Tab 在此处调用 initialize()

    if (name == "图像") {
        return new ImageTabWidget(m_pipelineManager, nullptr);
    }
    else if (name == "视频") {
        return new VideoTabWidget(nullptr);
    }
    else if (name == "增强") {
        return new EnhanceTabWidget(m_pipelineManager, nullptr);
    }
    else if (name == "过滤") {
        return new FilterTabWidget(m_pipelineManager, nullptr);
    }
    else if (name == "补正") {
        auto* tab = new TemplateTabWidget(m_view, m_roiManager, nullptr);
        tab->initialize();
        return tab;
    }
    else if (name == "处理") {
        auto* tab = new ProcessTabWidget(m_pipelineManager, nullptr);
        tab->initialize();
        return tab;
    }
    else if (name == "提取") {
        auto* tab = new ExtractTabWidget(m_pipelineManager, m_view, m_roiManager, nullptr);
        tab->initialize();
        return tab;
    }
    else if (name == "判定") {
        return new JudgeTabWidget(m_pipelineManager, nullptr);
    }
    else if (name == "直线") {
        auto debounceFunc = [this]() { m_debounceTimer->start(); };
        auto* tab = new LineDetectTabWidget(m_pipelineManager, debounceFunc, nullptr);
        tab->initialize();
        return tab;
    }
    else if (name == "条码") {
        auto debounceFunc = [this]() { m_debounceTimer->start(); };
        return new BarcodeTabWidget(m_pipelineManager, debounceFunc, nullptr);
    }
    else if (name == "目标检测") {
        return new ObjectDetectionTabWidget(m_pipelineManager, nullptr);
    }

    return nullptr;
}