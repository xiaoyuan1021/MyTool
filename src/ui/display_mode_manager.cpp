#include "display_mode_manager.h"
#include "core/pipeline_manager.h"
#include "ui/image_view.h"
#include "widgets/tab_registry.h"
#include "logger.h"

#include <QTabWidget>

DisplayModeManager::DisplayModeManager(QTabWidget* tabWidget,
                                       PipelineManager* pipelineManager,
                                       ImageView* view,
                                       QObject* parent)
    : QObject(parent)
    , m_tabWidget(tabWidget)
    , m_pipelineManager(pipelineManager)
    , m_view(view)
{
}

DisplayConfig::Mode DisplayModeManager::getModeForTab(int index) const
{
    if (index < 0 || index >= m_tabWidget->count()) {
        return DisplayConfig::Mode::Original;
    }
    QString tabName = m_tabWidget->tabText(index);

    // 从 TabRegistry 查询显示模式（单一数据源）
    return TabRegistry::instance().displayModeFor(tabName);
}

void DisplayModeManager::onTabChanged(int index)
{
    DisplayConfig::Mode newMode = getModeForTab(index);
    if (newMode != m_lastMode) {
        m_lastMode = newMode;
        if (!displayCurrentResult()) {
            // 没有上次Pipeline结果，需要触发完整处理
            emit requestRefresh();
        }
    }
}

void DisplayModeManager::applyModeForCurrentTab()
{
    int idx = m_tabWidget->currentIndex();
    if (idx < 0) return;
    QString tabName = m_tabWidget->tabText(idx);

    DisplayConfig::Mode mode = getModeForTab(idx);

    // "颜色过滤" Tab 的显示模式根据当前过滤配置动态决定
    if (tabName == "颜色过滤") {
        PipelineConfig cfg = m_pipelineManager->getConfigSnapshot();
        mode = (cfg.colorFilter.mode == ImageFilterMode::None)
            ? DisplayConfig::Mode::Original
            : DisplayConfig::Mode::MaskGreenWhite;
    }

    m_pipelineManager->setDisplayMode(mode);
}

bool DisplayModeManager::displayCurrentResult()
{
    if (!m_pipelineManager->hasLastResult()) {
        Logger::instance()->info("[DisplayMode] displayCurrentResult: 无缓存结果，需触发Pipeline");
        return false;  // 需要触发完整Pipeline处理
    }
    DisplayConfig::Mode mode = getModeForTab(m_tabWidget->currentIndex());
    cv::Mat displayImage = m_pipelineManager->getLastDisplayWithMode(mode);
    if (!displayImage.empty()) {
        qDebug() << "[DisplayMode] displayCurrentResult: 使用缓存渲染, mode=" << static_cast<int>(mode)
                 << "size=" << displayImage.cols << "x" << displayImage.rows;
        m_view->setImage(ImageUtils::matToQImage(displayImage));
    }
    return true;
}