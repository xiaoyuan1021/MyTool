#include "step_config_widget.h"
#include "core/pipeline_manager.h"
#include "core/roi_manager.h"
#include "image_view.h"
#include "controllers/roi_ui_controller.h"

#include <QLabel>
#include <QTabWidget>
#include <QHBoxLayout>
#include <QSet>
#include <QApplication>
#include <QDrag>
#include <QMimeData>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QMouseEvent>
#include <climits>

// ============================================================
// UI 步骤条目表 —— 新增/修改步骤只需改这里
//
// 字段说明：
//   displayName     复选框文字
//   tabNames        勾选后要显示/隐藏的右侧Tab名称
//   backendIndices  对应 PipelineConfig::stepEnabled[] 的下标
//                   （-1=无对应后端步骤，仅控制Tab显隐）
//   alwaysOn        固定勾选且不可拖拽
// ============================================================
const StepConfigWidget::StepEntry StepConfigWidget::kSteps[] = {
    {"颜色通道",      {"图像"},        {0},           false},
    {"图像增强",      {"增强"},        {1},           false},
    // 统一过滤（灰度/RGB/HSV）
    {"过滤",          {"过滤"},        {2},           false},
    {"算法处理",      {"处理"},        {3},           false},
    {"形状筛选",      {"提取"},        {4},           false},
    // 直线检测（含参考线匹配）
    {"直线检测",      {"直线"},        {5},           false},
    // 模板匹配——无对应后端步骤，仅控制 Tab 显隐
    {"模板匹配",      {"补正"},        {-1},          false},
    {"条码识别",      {"条码"},        {6},           false},
    // 滤波去噪——对应 StepImageFilter(7)
    {"滤波去噪",      {"滤波去噪"},    {7},           false},
    // 文字识别——对应 StepOcrRecognition(8)
    {"文字识别",      {"文字识别"},    {8},           false},
    // 目标检测——无对应后端步骤，通过 PipelineConfig::enableObjectDetection 控制
    {"目标检测",      {"目标检测"},    {-1},          false},

    // 步骤 Tab 自身始终显示（单独放在列表下方）
    {"步骤管理",      {"步骤"},        {-1},          true},
};

const int StepConfigWidget::kStepCount = sizeof(kSteps) / sizeof(kSteps[0]);

// ============================================================

static const char* kMimeType = "application/x-step-entry-index";

StepConfigWidget::StepConfigWidget(PipelineManager* pipelineManager,
                                   QWidget* parent)
    : QWidget(parent)
    , m_pipelineManager(pipelineManager)
{
    setupUI();
    rebuildStepItems();
}

void StepConfigWidget::connectSignals(PipelineManager* pm, RoiManager*,
                                      ImageView*, RoiUiController* roiCtrl,
                                      std::function<void()> requestRefresh,
                                      std::function<void()>)
{
    m_pipelineManager = pm;
    m_requestRefresh = std::move(requestRefresh);

    if (roiCtrl) {
        connect(roiCtrl, &RoiUiController::roiPipelineConfigChanged,
                this, [this](const PipelineConfig&) {
            rebuildStepItems();
        });
    }

    rebuildStepItems();
}

// ========== 事件过滤：实现拖拽 ==========

bool StepConfigWidget::eventFilter(QObject* obj, QEvent* event)
{
    // ---- 步骤项拖拽发起 ----
    if (m_stepFrames.contains(static_cast<QFrame*>(obj))) {
        auto* frame = static_cast<QFrame*>(obj);
        switch (event->type()) {
        case QEvent::MouseButtonPress: {
            auto* me = static_cast<QMouseEvent*>(event);
            if (me->button() == Qt::LeftButton) {
                m_dragSource = frame;
                m_dragStartPos = me->pos();
            }
            break;
        }
        case QEvent::MouseMove: {
            if (frame != m_dragSource) break;
            auto* me = static_cast<QMouseEvent*>(event);
            if (!(me->buttons() & Qt::LeftButton)) break;
            if ((me->pos() - m_dragStartPos).manhattanLength()
                < QApplication::startDragDistance()) break;

            int entryIdx = entryIndexForFrame(frame);
            if (entryIdx < 0) break;

            auto* drag = new QDrag(frame);
            auto* mime = new QMimeData();
            mime->setData(kMimeType, QByteArray::number(entryIdx));
            drag->setMimeData(mime);
            drag->exec(Qt::MoveAction);
            frame->setCursor(Qt::OpenHandCursor);
            m_dragSource = nullptr;
            break;
        }
        case QEvent::MouseButtonRelease: {
            frame->setCursor(Qt::OpenHandCursor);
            m_dragSource = nullptr;
            break;
        }
        default:
            break;
        }
        return QWidget::eventFilter(obj, event);
    }

    // ---- 容器拖放 ----
    if (obj == m_stepContainer) {
        switch (event->type()) {
        case QEvent::DragEnter: {
            auto* de = static_cast<QDragEnterEvent*>(event);
            if (de->mimeData()->hasFormat(kMimeType)) {
                de->acceptProposedAction();
            }
            break;
        }
        case QEvent::DragMove: {
            auto* dm = static_cast<QDragMoveEvent*>(event);
            if (dm->mimeData()->hasFormat(kMimeType)) {
                dm->acceptProposedAction();
            }
            break;
        }
        case QEvent::Drop: {
            auto* dp = static_cast<QDropEvent*>(event);
            if (!dp->mimeData()->hasFormat(kMimeType)) break;

            int draggedEntry = dp->mimeData()->data(kMimeType).toInt();
            int targetPos = dropTargetIndex(dp->position().toPoint());

            // 从 layout 中移除源 widget
            QFrame* sourceFrame = nullptr;
            int sourcePos = -1;
            for (int i = 0; i < m_stepFrames.size(); ++i) {
                if (entryIndexForFrame(m_stepFrames[i]) == draggedEntry) {
                    sourceFrame = m_stepFrames[i];
                    sourcePos = i;
                    break;
                }
            }
            if (!sourceFrame) break;

            m_stepLayout->removeWidget(sourceFrame);
            m_stepFrames.removeAt(sourcePos);

            // 计算插入位置（移除后调整）
            if (sourcePos < targetPos) --targetPos;
            targetPos = qBound(0, targetPos, m_stepFrames.size());

            m_stepLayout->insertWidget(targetPos, sourceFrame);
            m_stepFrames.insert(targetPos, sourceFrame);

            dp->acceptProposedAction();
            break;
        }
        default:
            break;
        }
        return QWidget::eventFilter(obj, event);
    }

    return QWidget::eventFilter(obj, event);
}

int StepConfigWidget::entryIndexForFrame(QObject* obj) const
{
    return obj->property("entryIndex").toInt();
}

int StepConfigWidget::dropTargetIndex(const QPoint& pos) const
{
    for (int i = 0; i < m_stepFrames.size(); ++i) {
        QRect r = m_stepFrames[i]->geometry();
        if (pos.y() < r.center().y()) return i;
    }
    return m_stepFrames.size();
}

// ========== UI 构建 ==========

void StepConfigWidget::setupUI()
{
    auto* mainLayout = new QVBoxLayout(this);

    auto* descLabel = new QLabel(
        QStringLiteral("勾选需要的处理步骤，拖拽可调整执行顺序，"
                       "点击「应用」后右侧动态加载对应参数Tab。"),
        this);
    descLabel->setWordWrap(true);
    mainLayout->addWidget(descLabel);

    // 可拖拽排序的步骤容器
    m_scrollArea = new QScrollArea(this);
    m_scrollArea->setWidgetResizable(true);
    m_scrollArea->setFrameShape(QFrame::NoFrame);

    m_stepContainer = new QWidget();
    m_stepContainer->setAcceptDrops(true);
    m_stepContainer->installEventFilter(this);
    m_stepLayout = new QVBoxLayout(m_stepContainer);
    m_stepLayout->setSpacing(2);
    m_stepLayout->setContentsMargins(0, 0, 0, 0);

    m_scrollArea->setWidget(m_stepContainer);
    mainLayout->addWidget(m_scrollArea, 1);

    // 固定条目（alwaysOn）显示在列表下方，不可拖拽
    m_fixedStepCb = new QCheckBox(QStringLiteral("步骤管理"), this);
    m_fixedStepCb->setChecked(true);
    m_fixedStepCb->setEnabled(false);
    mainLayout->addWidget(m_fixedStepCb);

    m_applyBtn = new QPushButton(QStringLiteral("应用"), this);
    m_applyBtn->setStyleSheet(
        QStringLiteral("QPushButton { font-weight: bold; padding: 8px; }"));
    mainLayout->addWidget(m_applyBtn);

    connect(m_applyBtn, &QPushButton::clicked, this, &StepConfigWidget::onApplyClicked);
}

void StepConfigWidget::rebuildStepItems()
{
    if (!m_pipelineManager) return;

    const auto& config = m_pipelineManager->config();

    // 提取可排序的条目索引（排除 alwaysOn）
    QList<int> sortableEntries;
    for (int i = 0; i < kStepCount; ++i) {
        if (!kSteps[i].alwaysOn) sortableEntries.append(i);
    }

    // 按 stepOrder 排序
    std::sort(sortableEntries.begin(), sortableEntries.end(),
              [&](int a, int b) {
        auto firstPos = [&](int entryIdx) -> int {
            for (int bi : kSteps[entryIdx].backendIndices) {
                if (bi < 0) continue;
                for (int p = 0; p < PipelineConfig::STEP_COUNT; ++p) {
                    if (config.stepOrder[p] == bi) return p;
                }
            }
            return INT_MAX;
        };
        return firstPos(a) < firstPos(b);
    });

    // 清除旧 widget
    m_dragSource = nullptr;
    for (auto* f : m_stepFrames) {
        f->removeEventFilter(this);
        f->deleteLater();
    }
    m_stepFrames.clear();

    // 清除 layout 中的所有 item（包括 spacer）
    while (m_stepLayout->count() > 0) {
        auto* item = m_stepLayout->takeAt(0);
        delete item;
    }

    // 创建新的步骤项
    for (int entryIdx : sortableEntries) {
        auto* frame = new QFrame();
        frame->setFrameShape(QFrame::StyledPanel);
        frame->setCursor(Qt::OpenHandCursor);

        QString style = QStringLiteral(
            "QFrame { background: white; border: 1px solid #D4E3F0;"
            " border-radius: 4px; padding: 4px; }"
            "QFrame:hover { background: #E8F0F8; }");
        frame->setStyleSheet(style);

        auto* hbox = new QHBoxLayout(frame);
        hbox->setContentsMargins(8, 4, 8, 4);

        auto* cb = new QCheckBox(
            QString::fromUtf8(kSteps[entryIdx].displayName), frame);

        bool anyEnabled = false;
        for (int idx : kSteps[entryIdx].backendIndices) {
            if (idx >= 0 && m_pipelineManager->isStepEnabled(idx)) {
                anyEnabled = true;
                break;
            }
        }
        // 虚拟步骤（backendIndex = -1）从 PipelineConfig 独立字段读取状态
        if (!anyEnabled && kSteps[entryIdx].backendIndices.size() == 1
            && kSteps[entryIdx].backendIndices[0] < 0
            && strcmp(kSteps[entryIdx].displayName, "目标检测") == 0) {
            anyEnabled = config.enableObjectDetection;
        }
        cb->setChecked(anyEnabled);
        hbox->addWidget(cb);
        hbox->addStretch();

        frame->setProperty("entryIndex", entryIdx);
        frame->installEventFilter(this);

        m_stepLayout->addWidget(frame);
        m_stepFrames.append(frame);
    }

    // 添加弹性空间
    m_stepLayout->addStretch();
}

// ========== 应用 ==========

void StepConfigWidget::onApplyClicked()
{
    if (!m_pipelineManager) return;

    // ---- 1) 从当前顺序读取勾选状态 + 排序 ----
    std::array<int, PipelineConfig::STEP_COUNT> newOrder{};
    QSet<int> placed;
    int pos = 0;

    for (auto* frame : m_stepFrames) {
        int entryIdx = entryIndexForFrame(frame);
        if (entryIdx < 0) continue;

        // 查找 frame 内的 QCheckBox
        auto* cb = frame->findChild<QCheckBox*>();
        bool checked = cb && cb->isChecked();

        for (int idx : kSteps[entryIdx].backendIndices) {
            if (idx < 0) continue;

            m_pipelineManager->setStepEnabled(idx, checked);

            if (!placed.contains(idx)) {
                newOrder[pos++] = idx;
                placed.insert(idx);
            }
        }

        // 当启用过滤步骤时，自动设置默认过滤模式（Gray）
        if (checked && kSteps[entryIdx].backendIndices.contains(2)) {
            auto& cf = m_pipelineManager->mutableConfig().colorFilter;
            if (cf.mode == ImageFilterMode::None) {
                cf.mode = ImageFilterMode::Gray;
            }
        }

        // 虚拟步骤：持久化到 PipelineConfig 独立字段
        if (kSteps[entryIdx].backendIndices.size() == 1
            && kSteps[entryIdx].backendIndices[0] < 0
            && strcmp(kSteps[entryIdx].displayName, "目标检测") == 0) {
            m_pipelineManager->mutableConfig().enableObjectDetection = checked;
        }
    }

    // 补全未出现的索引
    for (int i = 0; i < PipelineConfig::STEP_COUNT; ++i) {
        if (!placed.contains(i)) {
            newOrder[pos++] = i;
        }
    }

    auto& cfg = m_pipelineManager->mutableConfig();
    for (int i = 0; i < PipelineConfig::STEP_COUNT; ++i) {
        cfg.stepOrder[i] = newOrder[i];
    }

    // ---- 2) 重建 Pipeline ----
    m_pipelineManager->rebuildPipeline();

    // ---- 3) 按用户排序创建 Tab ----
    emit tabsNeeded(collectEnabledTabNames());

    // ---- 4) 动态显示/隐藏 ----
    applyTabVisibility();

    // ---- 5) 触发刷新 ----
    emit stepConfigChanged();
    if (m_requestRefresh) m_requestRefresh();
}

// ========== Tab 管理 ==========

QStringList StepConfigWidget::collectEnabledTabNames()
{
    QSet<QString> checked;
    for (auto* frame : m_stepFrames) {
        int entryIdx = entryIndexForFrame(frame);
        if (entryIdx < 0) continue;
        auto* cb = frame->findChild<QCheckBox*>();
        if (!cb || !cb->isChecked()) continue;

        for (const auto& name : kSteps[entryIdx].tabNames) {
            checked.insert(name);
        }
    }
    checked.insert(QStringLiteral("步骤"));

    // 按用户拖拽后的顺序输出（去重）
    QStringList result;
    for (auto* frame : m_stepFrames) {
        int entryIdx = entryIndexForFrame(frame);
        if (entryIdx < 0) continue;
        auto* cb = frame->findChild<QCheckBox*>();
        if (!cb || !cb->isChecked()) continue;

        for (const auto& name : kSteps[entryIdx].tabNames) {
            if (!result.contains(name)) {
                result.append(name);
            }
        }
    }
    // "步骤"始终最后
    result.removeAll(QStringLiteral("步骤"));
    result.append(QStringLiteral("步骤"));
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
