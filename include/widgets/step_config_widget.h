#pragma once

#include <QWidget>
#include <QCheckBox>
#include <QPushButton>
#include <QStringList>
#include <QList>
#include <functional>
#include "config/pipeline_config.h"
#include "widgets/i_tab_interfaces.h"

class PipelineManager;

/**
 * @brief 自定义Pipeline步骤配置Tab
 *
 * 每个 UI 步骤条目（StepEntry）对应：
 *  - 一个复选框（显示名）
 *  - 一个或多个要显示/隐藏的右侧Tab
 *  - 一个或多个后端 Pipeline 步骤索引（stepEnabled[] 下标，-1=无对应）
 *
 * 这样 UI 层与后端 StepType 枚举解耦，维护时只需增删 kSteps 数组即可。
 */
class StepConfigWidget : public QWidget, public ISignalConnectable
{
    Q_OBJECT

public:
    explicit StepConfigWidget(PipelineManager* pipelineManager,
                              QWidget* parent = nullptr);

    void connectSignals(PipelineManager* pm, RoiManager* rm,
                        ImageView* view, RoiUiController* roiCtrl,
                        std::function<void()> requestRefresh,
                        std::function<void()> processAndDisplay) override;

signals:
    void stepConfigChanged();
    void tabsNeeded(const QStringList& tabNames);

private slots:
    void onApplyClicked();

private:
    /// UI 步骤条目 —— 与后端 StepType 枚举解耦
    struct StepEntry {
        const char* displayName;         // 复选框文字
        QStringList   tabNames;          // 勾选后要显示的Tab名
        QList<int>    backendIndices;    // stepEnabled[] 下标（-1 = 无对应）
        bool          alwaysOn = false;  // 始终勾选且不可取消
    };

    void setupUI();
    void syncCheckboxes();
    QStringList collectEnabledTabNames();
    void applyTabVisibility();

    static const StepEntry kSteps[];   // 定义在 .cpp
    static const int       kStepCount;

    PipelineManager*         m_pipelineManager = nullptr;
    std::function<void()>    m_requestRefresh;
    QPushButton*             m_applyBtn = nullptr;
    QList<QCheckBox*>        m_checkboxes;      // 与 kSteps 一一对应
};
