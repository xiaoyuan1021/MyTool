#include "step_config_widget.h"
#include "core/pipeline_manager.h"
#include "core/roi_manager.h"
#include "image_view.h"
#include "controllers/roi_ui_controller.h"
#include <QVBoxLayout>
#include <QGroupBox>
#include <QScrollArea>
#include <QLabel>
#include <QTabWidget>
#include <QSet>

// ============================================================
// UI 步骤条目表 —— 新增/修改步骤只需改这里
//
// 字段说明：
//   displayName     复选框文字
//   tabNames        勾选后要显示/隐藏的右侧Tab名称
//   backendIndices  对应 PipelineConfig::stepEnabled[] 的下标
//                   （-1=无对应后端步骤，仅控制Tab显隐）
//   alwaysOn        固定勾选且不可取消
// ============================================================
const StepConfigWidget::StepEntry StepConfigWidget::kSteps[] = {
    // Pipeline 执行步骤（对应后端 StepType 枚举）
    {"颜色通道",      {"图像"},        {0},           false},
    {"图像增强",      {"增强"},        {1},           false},
    {"灰度过滤",      {"过滤"},        {2},           false},
    {"颜色过滤",      {"过滤"},        {3},           false},
    {"算法处理",      {"处理"},        {4},           false},
    {"形状筛选",      {"提取"},        {5},           false},
    // 直线检测——一个复选框同时启用 StepLineDetect(6) 和 StepReferenceLineFilter(7)
    {"直线检测",      {"直线"},        {6, 7},        false},
    // 模板匹配——无对应后端步骤（补正Tab已有自身的启用控制），仅控制 Tab 显隐
    {"模板匹配",      {"补正"},        {-1},          false},
    // 条码识别
    {"条码识别",      {"条码"},        {8},           false},

    // 步骤 Tab 自身始终显示
    {"步骤管理",      {"步骤"},        {-1},          true},
};

const int StepConfigWidget::kStepCount = sizeof(kSteps) / sizeof(kSteps[0]);

// ============================================================

StepConfigWidget::StepConfigWidget(PipelineManager* pipelineManager,
                                   QWidget* parent)
    : QWidget(parent)
    , m_pipelineManager(pipelineManager)
{
    setupUI();
    syncCheckboxes();
}

void StepConfigWidget::connectSignals(PipelineManager* pm, RoiManager*,
                                      ImageView*, RoiUiController* roiCtrl,
                                      std::function<void()> requestRefresh,
                                      std::function<void()>)
{
    m_pipelineManager = pm;
    m_requestRefresh = std::move(requestRefresh);

    // ROI 切换时同步复选框
    if (roiCtrl) {
        connect(roiCtrl, &RoiUiController::roiPipelineConfigChanged,
                this, [this](const PipelineConfig& config) {
            syncCheckboxes();
        });
    }

    syncCheckboxes();
}

void StepConfigWidget::setupUI()
{
    auto* mainLayout = new QVBoxLayout(this);

    auto* descLabel = new QLabel(
        QStringLiteral("勾选需要的处理步骤，点击「应用」后右侧动态加载对应参数Tab。"),
        this);
    descLabel->setWordWrap(true);
    mainLayout->addWidget(descLabel);

    auto* group = new QGroupBox(QStringLiteral("选择步骤"), this);
    auto* groupLayout = new QVBoxLayout(group);

    for (int i = 0; i < kStepCount; ++i) {
        auto* cb = new QCheckBox(kSteps[i].displayName, this);
        if (kSteps[i].alwaysOn) {
            cb->setChecked(true);
            cb->setEnabled(false);  // 不可取消
        }
        m_checkboxes.append(cb);
        groupLayout->addWidget(cb);
    }

    auto* scrollArea = new QScrollArea(this);
    scrollArea->setWidgetResizable(true);
    scrollArea->setWidget(group);
    scrollArea->setFrameShape(QFrame::NoFrame);
    mainLayout->addWidget(scrollArea, 1);

    m_applyBtn = new QPushButton(QStringLiteral("应用"), this);
    m_applyBtn->setStyleSheet(
        QStringLiteral("QPushButton { font-weight: bold; padding: 8px; }"));
    mainLayout->addWidget(m_applyBtn);

    connect(m_applyBtn, &QPushButton::clicked, this, &StepConfigWidget::onApplyClicked);
}

void StepConfigWidget::syncCheckboxes()
{
    if (!m_pipelineManager) return;

    for (int i = 0; i < kStepCount && i < m_checkboxes.size(); ++i) {
        if (kSteps[i].alwaysOn || kSteps[i].backendIndices.isEmpty()) {
            continue;  // 固定项不刷新
        }
        // 只要任一后端步骤启用，复选框就勾选
        bool anyEnabled = false;
        for (int idx : kSteps[i].backendIndices) {
            if (idx >= 0 && m_pipelineManager->isStepEnabled(idx)) {
                anyEnabled = true;
                break;
            }
        }
        m_checkboxes[i]->setChecked(anyEnabled);
    }
}

void StepConfigWidget::onApplyClicked()
{
    if (!m_pipelineManager) return;

    // 1) 将勾选状态写入 PipelineManager
    for (int i = 0; i < kStepCount; ++i) {
        for (int idx : kSteps[i].backendIndices) {
            if (idx >= 0) {
                m_pipelineManager->setStepEnabled(idx, m_checkboxes[i]->isChecked());
            }
        }
    }

    // 2) 重建 Pipeline
    m_pipelineManager->rebuildPipeline();

    // 3) 请求外部创建需要的 Tab（固定顺序创建）
    emit tabsNeeded(collectEnabledTabNames());

    // 4) 动态显示/隐藏对应的 Tab
    applyTabVisibility();

    // 5) 触发刷新
    emit stepConfigChanged();
    if (m_requestRefresh) m_requestRefresh();
}

QStringList StepConfigWidget::collectEnabledTabNames()
{
    // 固定顺序 —— 按 Pipeline 执行顺序排列
    QStringList order = {
        "图像", "增强", "过滤", "补正",
        "处理", "提取", "直线", "条码",
        "步骤"
    };

    // 收集已勾选条目对应的 Tab 名
    QSet<QString> checked;
    for (int i = 0; i < kStepCount; ++i) {
        if (m_checkboxes[i]->isChecked()) {
            for (const auto& name : kSteps[i].tabNames) {
                checked.insert(name);
            }
        }
    }
    // "步骤" 始终显示
    checked.insert(QStringLiteral("步骤"));

    // 按固定顺序输出
    QStringList result;
    for (const auto& name : order) {
        if (checked.contains(name)) {
            result.append(name);
        }
    }
    return result;
}

void StepConfigWidget::applyTabVisibility()
{
    QTabWidget* tabWidget = nullptr;
    QWidget* w = parentWidget();
    while (w) {
        tabWidget = qobject_cast<QTabWidget*>(w);
        if (tabWidget) break;
        w = w->parentWidget();
    }
    if (!tabWidget) return;

    QStringList enabledTabs = collectEnabledTabNames();
    QSet<QString> enabledSet(enabledTabs.begin(), enabledTabs.end());

    for (int i = 0; i < tabWidget->count(); ++i) {
        tabWidget->setTabVisible(i, enabledSet.contains(tabWidget->tabText(i)));
    }

    for (int i = 0; i < tabWidget->count(); ++i) {
        if (tabWidget->isTabVisible(i)) {
            tabWidget->setCurrentIndex(i);
            break;
        }
    }
}
