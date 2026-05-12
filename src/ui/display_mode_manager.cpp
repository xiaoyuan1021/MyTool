#include "display_mode_manager.h"
#include "core/pipeline_manager.h"
#include "ui/image_view.h"

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

    static const QHash<QString, DisplayConfig::Mode> tabDisplayMode = {
        {"图像",     DisplayConfig::Mode::Channel},
        {"视频",     DisplayConfig::Mode::Channel},
        {"增强",     DisplayConfig::Mode::Enhanced},
        {"补正",     DisplayConfig::Mode::Original},
        {"处理",     DisplayConfig::Mode::Processed},
        {"提取",     DisplayConfig::Mode::Processed},
        {"判定",     DisplayConfig::Mode::MaskOverlay},
        {"直线",     DisplayConfig::Mode::LineDetect},
        {"条码",     DisplayConfig::Mode::BarcodeOverlay},
        {"目标检测", DisplayConfig::Mode::Original},
    };

    return tabDisplayMode.value(tabName, DisplayConfig::Mode::Original);
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

    if (tabName == "过滤") {
        PipelineConfig cfg = m_pipelineManager->getConfigSnapshot();
        mode = (cfg.colorFilter.currentFilterMode == PipelineConfig::FilterMode::None)
            ? DisplayConfig::Mode::Original
            : DisplayConfig::Mode::MaskGreenWhite;
    }

    m_pipelineManager->setDisplayMode(mode);
}

bool DisplayModeManager::displayCurrentResult()
{
    if (!m_pipelineManager->hasLastResult()) {
        return false;  // 需要触发完整Pipeline处理
    }
    DisplayConfig::Mode mode = getModeForTab(m_tabWidget->currentIndex());
    cv::Mat displayImage = m_pipelineManager->getLastDisplayWithMode(mode);
    if (!displayImage.empty()) {
        m_view->setImage(ImageUtils::matToQImage(displayImage));
    }
    return true;
}
