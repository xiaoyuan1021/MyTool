#pragma once

#include <QWidget>
#include <QCheckBox>
#include <QPushButton>
#include <QFrame>
#include <QVBoxLayout>
#include <QScrollArea>
#include <QStringList>
#include <QList>
#include <QPoint>
#include <functional>
#include "config/pipeline_config.h"
#include "widgets/i_tab_interfaces.h"

class QLabel;
class PipelineManager;
class RoiManager;

/**
 * @brief 自定义Pipeline步骤配置Tab（支持拖拽排序）
 *
 * 每个 UI 步骤条目（StepEntry）对应：
 *  - 一个 QFrame（内含 QCheckBox）
 *  - 一个或多个要显示/隐藏的右侧Tab
 *  - 一个或多个后端 Pipeline 步骤索引（stepEnabled[] 下标，-1=无对应）
 *
 * 用户拖拽调整顺序 → 点击【应用】→ 按用户顺序执行。
 * UI 层与后端 StepType 枚举解耦，维护时只需增删 kSteps 数组。
 */
class StepConfigWidget : public QWidget, public ISignalConnectable, public IConfigurableTab
{
    Q_OBJECT

public:
    explicit StepConfigWidget(PipelineManager* pipelineManager,
                              QWidget* parent = nullptr);

    void connectSignals(const SignalContext& ctx,
                        std::function<void()> onExecutePipeline,
                        std::function<void()> onConfigSaved = nullptr) override;

    // IConfigurableTab 接口实现
    void saveToConfig(PipelineConfig& config) const override;
    void loadFromConfig(const PipelineConfig& config) override;

    /// 根据当前配置恢复步骤Tab的可见性（图片切换时调用）
    void applyTabVisibility();

signals:
    void tabsNeeded(const QStringList& tabNames);

private slots:
    void onApplyClicked();

protected:
    bool eventFilter(QObject* obj, QEvent* event) override;

private:
    /// UI 步骤条目 —— 与后端 StepType 枚举解耦
    struct StepEntry {
        const char* displayName;         // 复选框文字
        QStringList   tabNames;          // 勾选后要显示的Tab名
        QList<int>    backendIndices;    // stepEnabled[] 下标（-1 = 无对应）
        bool          alwaysOn = false;  // 始终勾选且不可拖拽
        const char*   icon = "";         // 步骤图标（Unicode符号）
        const char*   description = "";  // 步骤描述
        const char*   color = "#4FACFE"; // 步骤主题色
    };

    void setupUI();
    void rebuildStepItems();
    void updatePipelinePreview();
    QStringList collectEnabledTabNames();
    static int entryIndexForFrame(QObject* obj);
    int  dropTargetIndex(const QPoint& pos) const;
    void moveStepToSelected(QFrame* frame);
    void moveStepToAvailable(QFrame* frame);
    void updateSelectedNumbers();
    void syncSelectedLayout();
    void updateAvailableAreaHeight();
    static void updateCardStyle(QFrame* frame, bool enabled);
    static QFrame* createStepCard(int entryIdx, bool anyEnabled, const StepEntry& step);

    static const StepEntry kSteps[];
    static const int       kStepCount;

    PipelineManager*         m_pipelineManager = nullptr;
    RoiManager*              m_roiManager = nullptr;
    std::function<void()>    m_onExecutePipeline;
    QPushButton*             m_applyBtn = nullptr;
    QLabel*                  m_previewStepsLabel = nullptr;

    // 待选区
    QScrollArea*              m_availableScroll = nullptr;
    QWidget*                 m_availableContainer = nullptr;
    QVBoxLayout*             m_availableOuterLayout = nullptr;
    QList<QFrame*>           m_availableFrames;

    // 确认区
    QWidget*                 m_selectedContainer = nullptr;
    QVBoxLayout*             m_selectedOuterLayout = nullptr;
    QList<QFrame*>           m_selectedFrames;
    QLabel*                  m_emptyHintLabel = nullptr;

    // 拖拽辅助
    QPoint                   m_dragStartPos;
    QFrame*                  m_dragSource = nullptr;
};
