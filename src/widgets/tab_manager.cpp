#include "widgets/tab_manager.h"
#include "widgets/tab_registry.h"
#include "pipeline_manager.h"
#include "roi_manager.h"
#include "image_view.h"
#include "logger.h"

TabManager::TabManager(
    QTabWidget* tabWidget,
    PipelineManager* pipelineManager,
    RoiManager* roiManager,
    ImageView* view,
    QObject* parent)
    : QObject(parent)
    , m_tabWidget(tabWidget)
    , m_pipelineManager(pipelineManager)
    , m_roiManager(roiManager)
    , m_view(view)
{
}

TabManager::~TabManager() = default;

void TabManager::ensureTab(const QString& tabName)
{
    if (isCreated(tabName)) return;

    // 通过 TabRegistry 创建 Tab（工厂逻辑集中在那里）
    QWidget* widget = TabRegistry::instance().createTab(
        tabName, m_pipelineManager, m_view, m_roiManager);
    if (!widget) return;

    m_tabWidget->addTab(widget, tabName);
    m_tabs[tabName] = widget;
    m_tabOrder.append(tabName);

    emit tabCreated(tabName, widget);
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