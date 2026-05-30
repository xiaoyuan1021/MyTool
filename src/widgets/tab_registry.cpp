#include "widgets/tab_registry.h"

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
#include "widgets/step_config_widget.h"
#include "widgets/image_filter_tab_widget.h"
#include "widgets/ocr_tab_widget.h"

TabRegistry& TabRegistry::instance()
{
    static TabRegistry reg;
    return reg;
}

TabRegistry::TabRegistry()
{
    // ====== 在此注册所有 Tab ======
    // 新增 Tab 时只需在此加一行 registerTab 调用

    registerTab(
        {"图像",     DisplayConfig::Mode::Channel,        false},
        [](PipelineManager* pm, ImageView*, RoiManager*, QStatusBar*) -> QWidget* {
            return new ImageTabWidget(pm, nullptr);
        }
    );

    registerTab(
        {"视频",     DisplayConfig::Mode::Channel,        false},
        [](PipelineManager*, ImageView*, RoiManager*, QStatusBar*) -> QWidget* {
            return new VideoTabWidget(nullptr);
        }
    );

    registerTab(
        {"增强",     DisplayConfig::Mode::Enhanced,       false},
        [](PipelineManager* pm, ImageView*, RoiManager*, QStatusBar*) -> QWidget* {
            return new EnhanceTabWidget(pm, nullptr);
        }
    );

    registerTab(
        {"颜色过滤",     DisplayConfig::Mode::MaskGreenWhite, false},
        [](PipelineManager* pm, ImageView*, RoiManager*, QStatusBar*) -> QWidget* {
            return new FilterTabWidget(pm, nullptr);
        }
    );

    registerTab(
        {"滤波去噪", DisplayConfig::Mode::Enhanced,       false},
        [](PipelineManager* pm, ImageView*, RoiManager*, QStatusBar*) -> QWidget* {
            return new ImageFilterTabWidget(pm, nullptr);
        }
    );

    registerTab(
        {"文字识别", DisplayConfig::Mode::OcrOverlay,   false},
        [](PipelineManager* pm, ImageView*, RoiManager*, QStatusBar*) -> QWidget* {
            return new OcrTabWidget(pm, nullptr);
        }
    );

    registerTab(
        {"补正",     DisplayConfig::Mode::Original,       true},
        [](PipelineManager*, ImageView* view, RoiManager* rm, QStatusBar*) -> QWidget* {
            auto* tab = new TemplateTabWidget(view, rm, nullptr);
            tab->initialize();
            return tab;
        }
    );

    registerTab(
        {"处理",     DisplayConfig::Mode::Processed,      true},
        [](PipelineManager* pm, ImageView*, RoiManager*, QStatusBar*) -> QWidget* {
            auto* tab = new ProcessTabWidget(pm, nullptr);
            tab->initialize();
            return tab;
        }
    );

    registerTab(
        {"提取",     DisplayConfig::Mode::Processed,      true},
        [](PipelineManager* pm, ImageView* view, RoiManager* rm, QStatusBar* statusBar) -> QWidget* {
            auto* tab = new ExtractTabWidget(pm, view, rm, statusBar, nullptr);
            tab->initialize();
            return tab;
        }
    );

    registerTab(
        {"判定",     DisplayConfig::Mode::MaskOverlay,    false},
        [](PipelineManager* pm, ImageView*, RoiManager*, QStatusBar*) -> QWidget* {
            return new JudgeTabWidget(pm, nullptr);
        }
    );

    registerTab(
        {"直线",     DisplayConfig::Mode::LineDetect,     true},
        [](PipelineManager* pm, ImageView*, RoiManager*, QStatusBar*) -> QWidget* {
            auto* tab = new LineDetectTabWidget(pm, nullptr);
            tab->initialize();
            return tab;
        }
    );

    registerTab(
        {"条码",     DisplayConfig::Mode::BarcodeOverlay, true},
        [](PipelineManager* pm, ImageView*, RoiManager*, QStatusBar*) -> QWidget* {
            return new BarcodeTabWidget(pm, nullptr);
        }
    );

    registerTab(
        {"目标检测", DisplayConfig::Mode::Original,       false},
        [](PipelineManager* pm, ImageView*, RoiManager*, QStatusBar*) -> QWidget* {
            return new ObjectDetectionTabWidget(pm, nullptr);
        }
    );

    registerTab(
        {"步骤",     DisplayConfig::Mode::Original,       false},
        [](PipelineManager* pm, ImageView*, RoiManager*, QStatusBar*) -> QWidget* {
            return new StepConfigWidget(pm, nullptr);
        }
    );
}

void TabRegistry::registerTab(const TabEntry& entry, FactoryFunc factory)
{
    m_nameIndex[entry.name] = m_entries.size();
    m_entries.append(entry);
    m_factories[entry.name] = std::move(factory);
}

const TabEntry* TabRegistry::find(const QString& name) const
{
    auto it = m_nameIndex.find(name);
    if (it == m_nameIndex.end()) return nullptr;
    return &m_entries[it.value()];
}

DisplayConfig::Mode TabRegistry::displayModeFor(const QString& name) const
{
    const TabEntry* entry = find(name);
    return entry ? entry->displayMode : DisplayConfig::Mode::Original;
}

QWidget* TabRegistry::createTab(const QString& name,
                                PipelineManager* pm, ImageView* view,
                                RoiManager* rm, QStatusBar* statusBar) const
{
    auto it = m_factories.find(name);
    if (it == m_factories.end()) return nullptr;
    return it.value()(pm, view, rm, statusBar);
}