#include "step_config_widget.h"
#include "core/pipeline_manager.h"
#include "core/roi_manager.h"
#include "image_view.h"
#include "controllers/roi_ui_controller.h"

#include <QLabel>
#include <QTabWidget>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QSet>
#include <QMap>
#include <QApplication>
#include <QDrag>
#include <QMimeData>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QMouseEvent>
#include <QFrame>
#include <climits>

// ============================================================
// UI 步骤条目表 —— 新增/修改步骤只需改这里
// ============================================================
const StepConfigWidget::StepEntry StepConfigWidget::kSteps[] = {
    {"颜色通道",      {"图像"},        {0},           false, "CH", "选择颜色通道：Gray/RGB/HSV", "#4FACFE"},
    {"图像增强",      {"增强"},        {1},           false, "EN", "亮度/对比度/Gamma/锐化", "#7C4DFF"},
    {"颜色过滤",      {"颜色过滤"},    {2},           false, "FT", "灰度/RGB/HSV范围过滤", "#00BFA5"},
    {"算法处理",      {"处理"},        {3},           false, "AL", "形态学操作：开闭运算/膨胀腐蚀", "#FF6D00"},
    {"形状筛选",      {"提取"},        {4},           false, "SF", "面积/圆度/凸性/矩形度筛选", "#E91E63"},
    {"直线检测",      {"直线"},        {5},           false, "LD", "HoughP/LSD/EDline直线检测", "#2196F3"},
    {"模板匹配",      {"补正"},        {-1},          false, "TM", "模板匹配定位与对齐", "#9C27B0"},
    {"条码识别",      {"条码"},        {6},           false, "BC", "QR Code/条形码识别", "#4CAF50"},
    {"滤波去噪",      {"滤波去噪"},    {7},           false, "FN", "高斯/中值/双边滤波去噪", "#00BCD4"},
    {"文字识别",      {"文字识别"},    {8},           false, "OC", "Tesseract OCR文字识别", "#FF5722"},
    {"目标检测",      {"目标检测"},    {-1},          false, "DL", "YOLO深度学习目标检测", "#607D8B"},
    {"步骤管理",      {"步骤"},        {-1},          true,  "PM", "管理Pipeline步骤顺序", "#795548"},
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

void StepConfigWidget::connectSignals(const SignalContext& ctx,
                                       std::function<void()> onExecutePipeline,
                                       std::function<void()> onConfigSaved)
{
    Q_UNUSED(onConfigSaved);
    m_pipelineManager = ctx.pipelineManager;
    m_roiManager = ctx.roiManager;
    m_onExecutePipeline = std::move(onExecutePipeline);

    if (ctx.roiCtrl) {
        connect(ctx.roiCtrl, &RoiUiController::roiPipelineConfigChanged,
                this, [this](const PipelineConfig&) {
            rebuildStepItems();
        });
    }

    rebuildStepItems();
}

// ========== 辅助函数 ==========

int StepConfigWidget::entryIndexForFrame(QObject* obj)
{
    return obj->property("entryIndex").toInt();
}

void StepConfigWidget::updateSelectedNumbers()
{
    for (int i = 0; i < m_selectedFrames.size(); ++i) {
        auto* numLabel = m_selectedFrames[i]->findChild<QLabel*>("numberLabel");
        if (numLabel) {
            numLabel->setText(QString::number(i + 1));
        }
    }
    m_emptyHintLabel->setVisible(m_selectedFrames.isEmpty());
}

// ========== 同步确认区 layout 与 m_selectedFrames ==========

void StepConfigWidget::syncSelectedLayout()
{
    // 移除所有 frame（保留 m_emptyHintLabel 和 stretch）
    QList<QFrame*> framesToRemove;
    for (int i = 0; i < m_selectedOuterLayout->count(); ++i) {
        auto* item = m_selectedOuterLayout->itemAt(i);
        if (item && item->widget()) {
            auto* frame = qobject_cast<QFrame*>(item->widget());
            if (frame && frame != m_emptyHintLabel) {
                framesToRemove.append(frame);
            }
        }
    }
    for (auto* frame : framesToRemove) {
        m_selectedOuterLayout->removeWidget(frame);
    }

    // 移除 stretch
    while (m_selectedOuterLayout->count() > 0) {
        auto* lastItem = m_selectedOuterLayout->itemAt(m_selectedOuterLayout->count() - 1);
        if (lastItem && lastItem->spacerItem()) {
            m_selectedOuterLayout->takeAt(m_selectedOuterLayout->count() - 1);
        } else {
            break;
        }
    }

    // 重新添加 frame（在 m_emptyHintLabel 之后）
    for (auto* frame : m_selectedFrames) {
        m_selectedOuterLayout->addWidget(frame);
    }

    // 重新添加 stretch
    m_selectedOuterLayout->addStretch();

    // 更新序号
    updateSelectedNumbers();
}

// ========== 更新可选步骤区域高度 ==========

void StepConfigWidget::updateAvailableAreaHeight()
{
    if (!m_availableScroll) return;

    int count = m_availableFrames.size();
    if (count == 0) {
        m_availableScroll->setMinimumHeight(0);
        m_availableScroll->setMaximumHeight(0);
        return;
    }

    // 每行2列，计算行数
    int rows = (count + 1) / 2;
    // 每个卡片高度40px，间距6px，加上一些padding
    int rowHeight = 40 + 6;
    int totalHeight = rows * rowHeight + 6;

    m_availableScroll->setMinimumHeight(totalHeight);
    m_availableScroll->setMaximumHeight(totalHeight);
}

// ========== 安全清理 layout（取出所有 widget 并销毁子 layout）==========

static void clearLayout(QVBoxLayout* outerLayout)
{
    while (outerLayout->count() > 0) {
        outerLayout->takeAt(0);
    }
}

void StepConfigWidget::moveStepToSelected(QFrame* frame)
{
    int entryIdx = entryIndexForFrame(frame);
    QString stepColor = QString::fromUtf8(kSteps[entryIdx].color);

    m_availableFrames.removeOne(frame);

    // 安全清理并重建待选区网格
    clearLayout(m_availableOuterLayout);

    QGridLayout* newGrid = new QGridLayout();
    newGrid->setSpacing(6);
    newGrid->setContentsMargins(0, 0, 0, 0);
    for (int i = 0; i < m_availableFrames.size(); ++i) {
        newGrid->addWidget(m_availableFrames[i], i / 2, i % 2);
    }
    m_availableOuterLayout->addLayout(newGrid);

    // 确认区：显示拖拽手柄和编号
    if (auto* handle = frame->findChild<QLabel*>("dragHandle")) {
        handle->show();
    }
    if (auto* numLabel = frame->findChild<QLabel*>("numberLabel")) {
        numLabel->show();
    }

    // 更新卡片样式为启用状态
    updateCardStyle(frame, true);

    // 已选步骤卡片：固定高度，宽度铺满
    frame->setMinimumWidth(0);
    frame->setMaximumWidth(QWIDGETSIZE_MAX);
    frame->setFixedHeight(40);

    // 注册拖拽事件
    frame->installEventFilter(this);

    // 添加到确认区列表末尾
    m_selectedFrames.append(frame);

    // 同步 layout
    syncSelectedLayout();
    updateAvailableAreaHeight();

    updatePipelinePreview();
}

void StepConfigWidget::moveStepToAvailable(QFrame* frame)
{
    int entryIdx = entryIndexForFrame(frame);
    QString stepColor = QString::fromUtf8(kSteps[entryIdx].color);

    m_selectedFrames.removeOne(frame);

    // 取消拖拽事件
    frame->removeEventFilter(this);

    // 待选区：隐藏拖拽手柄和编号
    if (auto* handle = frame->findChild<QLabel*>("dragHandle")) {
        handle->hide();
    }
    if (auto* numLabel = frame->findChild<QLabel*>("numberLabel")) {
        numLabel->hide();
    }

    // 更新卡片样式为禁用状态
    updateCardStyle(frame, false);

    // 恢复可选步骤卡片的固定大小
    frame->setFixedSize(200, 40);

    // 安全清理并重建待选区网格
    m_availableFrames.append(frame);
    clearLayout(m_availableOuterLayout);

    QGridLayout* newGrid = new QGridLayout();
    newGrid->setSpacing(6);
    newGrid->setContentsMargins(0, 0, 0, 0);
    for (int i = 0; i < m_availableFrames.size(); ++i) {
        newGrid->addWidget(m_availableFrames[i], i / 2, i % 2);
    }
    m_availableOuterLayout->addLayout(newGrid);

    // 同步确认区 layout
    syncSelectedLayout();
    updateAvailableAreaHeight();

    updatePipelinePreview();
}

// ========== 事件过滤：实现拖拽（仅确认区） ==========

bool StepConfigWidget::eventFilter(QObject* obj, QEvent* event)
{
    // ---- 确认区步骤项拖拽发起 ----
    if (m_selectedFrames.contains(static_cast<QFrame*>(obj))) {
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

            // 执行拖拽并检查结果
            Qt::DropAction result = drag->exec(Qt::MoveAction);
            frame->setCursor(Qt::OpenHandCursor);

            // 只有在没有执行 MoveAction 时才清空 m_dragSource
            // 如果执行了 MoveAction，Drop 事件处理器会处理状态
            if (result != Qt::MoveAction) {
                m_dragSource = nullptr;
            }
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

    // ---- 确认区容器拖放 ----
    if (obj == m_selectedContainer) {
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

            QFrame* sourceFrame = nullptr;
            int sourcePos = -1;
            for (int i = 0; i < m_selectedFrames.size(); ++i) {
                if (entryIndexForFrame(m_selectedFrames[i]) == draggedEntry) {
                    sourceFrame = m_selectedFrames[i];
                    sourcePos = i;
                    break;
                }
            }
            if (!sourceFrame) break;

            // 从原位置移除
            m_selectedFrames.removeAt(sourcePos);

            // 调整目标位置（如果源位置在目标之前，目标索引需要减1）
            if (sourcePos < targetPos) --targetPos;
            targetPos = qBound(0, targetPos, m_selectedFrames.size());

            // 插入到新位置
            m_selectedFrames.insert(targetPos, sourceFrame);

            // 同步 layout
            syncSelectedLayout();

            dp->acceptProposedAction();
            updatePipelinePreview();

            // 清空拖拽状态
            m_dragSource = nullptr;
            break;
        }
        default:
            break;
        }
        return QWidget::eventFilter(obj, event);
    }

    return QWidget::eventFilter(obj, event);
}

int StepConfigWidget::dropTargetIndex(const QPoint& pos) const
{
    if (m_selectedFrames.isEmpty()) return 0;

    for (int i = 0; i < m_selectedFrames.size(); ++i) {
        QRect r = m_selectedFrames[i]->geometry();
        // 使用 frame 的上半部分和下半部分来判断
        // 如果 pos 在 frame 的上 1/3 区域，插入到该 frame 之前
        // 如果 pos 在 frame 的下 1/3 区域，插入到该 frame 之后
        // 中间 1/3 区域根据垂直位置判断
        int topThird = r.top() + r.height() / 3;
        int bottomThird = r.bottom() - r.height() / 3;

        if (pos.y() < topThird) {
            return i;
        } else if (pos.y() < bottomThird) {
            // 在中间区域，使用中心点判断
            if (pos.y() < r.center().y()) {
                return i;
            }
        }
    }
    return m_selectedFrames.size();
}

// ========== UI 构建 ==========

static const char* scrollBarStyle =
    "QScrollBar:vertical { background-color: transparent; width: 6px; }"
    "QScrollBar::handle:vertical { background-color: #C8D4E0; border-radius: 3px; min-height: 24px; }"
    "QScrollBar::handle:vertical:hover { background-color: #4FACFE; }";

void StepConfigWidget::setupUI()
{
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(6);
    mainLayout->setContentsMargins(8, 8, 8, 8);

    // ---- 标题 ----
    auto* headerLayout = new QHBoxLayout();
    headerLayout->setSpacing(8);

    auto* pipelineIcon = new QLabel("P", this);
    pipelineIcon->setFixedSize(28, 28);
    pipelineIcon->setAlignment(Qt::AlignCenter);
    pipelineIcon->setStyleSheet(
        "QLabel { background-color: #4FACFE; color: white; "
        "border-radius: 14px; font-size: 14px; font-weight: bold; }");
    headerLayout->addWidget(pipelineIcon);

    auto* titleLabel = new QLabel(QStringLiteral("检测流程配置"), this);
    titleLabel->setStyleSheet(
        "QLabel { font-size: 14px; font-weight: bold; color: #1E293B; background: transparent; }");
    headerLayout->addWidget(titleLabel);
    headerLayout->addStretch();

    mainLayout->addLayout(headerLayout);

    // ---- 说明 ----
    auto* descLabel = new QLabel(
        QStringLiteral("勾选需要的步骤，拖拽调整执行顺序，点击「应用」生效"),
        this);
    descLabel->setStyleSheet(
        "QLabel { color: #6B7280; font-size: 11px; padding: 0 0 0 36px; background: transparent; }");
    descLabel->setWordWrap(true);
    mainLayout->addWidget(descLabel);

    // ---- 流程预览 ----
    auto* previewFrame = new QFrame(this);
    previewFrame->setAutoFillBackground(true);
    QPalette previewPal = previewFrame->palette();
    previewPal.setColor(QPalette::Window, QColor("#F1F5F9"));
    previewFrame->setPalette(previewPal);
    previewFrame->setStyleSheet(
        "QFrame { border: 1px solid #E2E8F0; border-radius: 8px; }");
    auto* previewLayout = new QHBoxLayout(previewFrame);
    previewLayout->setSpacing(6);
    previewLayout->setContentsMargins(10, 8, 10, 8);

    auto* inputBadge = new QLabel("输入", previewFrame);
    inputBadge->setAutoFillBackground(true);
    QPalette badgePal;
    badgePal.setColor(QPalette::Window, QColor("#4FACFE"));
    badgePal.setColor(QPalette::WindowText, Qt::white);
    inputBadge->setPalette(badgePal);
    inputBadge->setAlignment(Qt::AlignCenter);
    inputBadge->setFixedWidth(48);
    inputBadge->setStyleSheet("QLabel { border-radius: 10px; font-size: 11px; font-weight: bold; }");
    previewLayout->addWidget(inputBadge);

    auto* arrow1 = new QLabel("->", previewFrame);
    arrow1->setStyleSheet("QLabel { color: #4FACFE; font-size: 14px; font-weight: bold; background: transparent; }");
    previewLayout->addWidget(arrow1);

    m_previewStepsLabel = new QLabel("...", previewFrame);
    m_previewStepsLabel->setTextFormat(Qt::RichText);
    m_previewStepsLabel->setStyleSheet("QLabel { background: transparent; line-height: 180%; }");
    m_previewStepsLabel->setWordWrap(true);
    previewLayout->addWidget(m_previewStepsLabel, 1);

    auto* arrow2 = new QLabel("->", previewFrame);
    arrow2->setStyleSheet("QLabel { color: #4FACFE; font-size: 14px; font-weight: bold; background: transparent; }");
    previewLayout->addWidget(arrow2);

    auto* outputBadge = new QLabel("输出", previewFrame);
    outputBadge->setAutoFillBackground(true);
    QPalette outPal;
    outPal.setColor(QPalette::Window, QColor("#10B981"));
    outPal.setColor(QPalette::WindowText, Qt::white);
    outputBadge->setPalette(outPal);
    outputBadge->setAlignment(Qt::AlignCenter);
    outputBadge->setFixedWidth(48);
    outputBadge->setStyleSheet("QLabel { border-radius: 10px; font-size: 11px; font-weight: bold; }");
    previewLayout->addWidget(outputBadge);

    mainLayout->addWidget(previewFrame);

    // ================================================================
    //  待选区
    // ================================================================
    auto* availTitle = new QLabel(QStringLiteral("▼ 可选步骤"), this);
    availTitle->setStyleSheet(
        "QLabel { font-size: 11px; font-weight: bold; color: #475569; "
        "padding: 2px 0; background: transparent; }");
    mainLayout->addWidget(availTitle);

    auto* availScroll = new QScrollArea(this);
    availScroll->setWidgetResizable(true);
    availScroll->setFrameShape(QFrame::NoFrame);
    availScroll->setStyleSheet(QString(
        "QScrollArea { background: transparent; border: none; }") + scrollBarStyle);
    m_availableScroll = availScroll;

    m_availableContainer = new QWidget();
    m_availableContainer->setAutoFillBackground(true);
    QPalette availPal;
    availPal.setColor(QPalette::Window, Qt::transparent);
    m_availableContainer->setPalette(availPal);

    m_availableOuterLayout = new QVBoxLayout(m_availableContainer);
    m_availableOuterLayout->setSpacing(0);
    m_availableOuterLayout->setContentsMargins(0, 0, 0, 0);

    availScroll->setWidget(m_availableContainer);
    mainLayout->addWidget(availScroll);

    // ================================================================
    //  确认区
    // ================================================================
    auto* selTitle = new QLabel(QStringLiteral("▼ 已选步骤 (拖拽排序)"), this);
    selTitle->setStyleSheet(
        "QLabel { font-size: 11px; font-weight: bold; color: #475569; "
        "padding: 2px 0; background: transparent; }");
    mainLayout->addWidget(selTitle);

    auto* selScroll = new QScrollArea(this);
    selScroll->setWidgetResizable(true);
    selScroll->setFrameShape(QFrame::NoFrame);
    selScroll->setStyleSheet(QString(
        "QScrollArea { background: transparent; border: none; }") + scrollBarStyle);

    m_selectedContainer = new QWidget();
    m_selectedContainer->setAcceptDrops(true);
    m_selectedContainer->installEventFilter(this);
    m_selectedContainer->setAutoFillBackground(true);
    QPalette selPal;
    selPal.setColor(QPalette::Window, Qt::transparent);
    m_selectedContainer->setPalette(selPal);

    m_selectedOuterLayout = new QVBoxLayout(m_selectedContainer);
    m_selectedOuterLayout->setSpacing(4);
    m_selectedOuterLayout->setContentsMargins(0, 0, 0, 0);

    // 空状态提示
    m_emptyHintLabel = new QLabel("请从上方勾选需要的步骤", m_selectedContainer);
    m_emptyHintLabel->setAlignment(Qt::AlignCenter);
    m_emptyHintLabel->setStyleSheet(
        "QLabel { color: #94A3B8; font-size: 11px; padding: 20px 0; background: transparent; }");
    m_selectedOuterLayout->addWidget(m_emptyHintLabel);

    selScroll->setWidget(m_selectedContainer);
    mainLayout->addWidget(selScroll, 1);

    // ---- 应用按钮 ----
    m_applyBtn = new QPushButton(QStringLiteral("应用配置"), this);
    m_applyBtn->setMinimumHeight(32);
    m_applyBtn->setAutoFillBackground(true);
    QPalette btnPal;
    btnPal.setColor(QPalette::Button, QColor("#4FACFE"));
    btnPal.setColor(QPalette::ButtonText, Qt::white);
    m_applyBtn->setPalette(btnPal);
    m_applyBtn->setStyleSheet(
        "QPushButton { "
        "  border: none; border-radius: 6px;"
        "  font-weight: bold; font-size: 12px; padding: 6px 16px; "
        "  background-color: #4FACFE; color: white; "
        "}"
        "QPushButton:hover { background-color: #3D9AF0; }"
        "QPushButton:pressed { background-color: #2A85D8; }"
        "QPushButton:disabled { background-color: #B8D4E8; }");
    mainLayout->addWidget(m_applyBtn);

    connect(m_applyBtn, &QPushButton::clicked, this, &StepConfigWidget::onApplyClicked);
}

// ========== 创建步骤卡片 ==========

QFrame* StepConfigWidget::createStepCard(int entryIdx, bool anyEnabled, const StepConfigWidget::StepEntry& step)
{
    auto* frame = new QFrame();
    frame->setObjectName("stepCard");

    // 可选步骤固定大小，已选步骤后面会调整
    if (!anyEnabled) {
        frame->setFixedSize(200, 40);
    }

    QString stepColor = QString::fromUtf8(step.color);
    QString borderColor = anyEnabled ? stepColor : "#CBD5E1";
    QString borderWidth = anyEnabled ? "2px" : "1px";

    frame->setAutoFillBackground(true);
    QPalette pal = frame->palette();
    pal.setColor(QPalette::Window, anyEnabled ? QColor("#FFFFFF") : QColor("#F1F5F9"));
    frame->setPalette(pal);

    frame->setStyleSheet(QString(
        "QFrame#stepCard { border: %1 solid %2; border-radius: 8px; }"
        "QFrame#stepCard:hover { border-color: %3; }"
    ).arg(borderWidth, borderColor, stepColor));

    auto* hbox = new QHBoxLayout(frame);
    hbox->setContentsMargins(8, 4, 8, 4);
    hbox->setSpacing(6);

    // 拖拽手柄（确认区可见，待选区隐藏）
    auto* dragHandle = new QLabel("::", frame);
    dragHandle->setObjectName("dragHandle");
    dragHandle->setStyleSheet(
        "QLabel { color: #94A3B8; font-size: 12px; padding: 0 2px; background: transparent; }");
    dragHandle->setCursor(Qt::OpenHandCursor);
    dragHandle->setToolTip("拖拽调整顺序");
    hbox->addWidget(dragHandle);

    // 编号圆圈（确认区可见，待选区隐藏）
    auto* numberLabel = new QLabel("0", frame);
    numberLabel->setObjectName("numberLabel");
    numberLabel->setAlignment(Qt::AlignCenter);
    numberLabel->setFixedSize(22, 22);
    numberLabel->setStyleSheet(QString(
        "QLabel { background-color: %1; color: white; border-radius: 11px; "
        "font-weight: bold; font-size: 10px; }"
    ).arg(anyEnabled ? stepColor : "#94A3B8"));
    hbox->addWidget(numberLabel);

    // 图标
    auto* iconLabel = new QLabel(QString::fromUtf8(step.icon), frame);
    iconLabel->setObjectName("iconLabel");
    iconLabel->setAlignment(Qt::AlignCenter);
    iconLabel->setFixedSize(24, 16);
    iconLabel->setStyleSheet(QString(
        "QLabel { background-color: %1; color: white; border-radius: 3px; "
        "font-size: 8px; font-weight: bold; }"
    ).arg(anyEnabled ? stepColor : "#94A3B8"));
    hbox->addWidget(iconLabel);

    // 名称
    auto* nameLabel = new QLabel(QString::fromUtf8(step.displayName), frame);
    nameLabel->setObjectName("nameLabel");
    nameLabel->setStyleSheet(QString(
        "QLabel { font-weight: bold; font-size: 11px; color: %1; background: transparent; }"
    ).arg(anyEnabled ? "#1E293B" : "#475569"));
    hbox->addWidget(nameLabel, 1);

    // 复选框
    auto* cb = new QCheckBox(frame);
    cb->setChecked(anyEnabled);
    cb->setToolTip(anyEnabled ? "已启用" : "点击启用");
    cb->setStyleSheet(QString(
        "QCheckBox { spacing: 4px; background: transparent; }"
        "QCheckBox::indicator { "
        "  width: 16px; height: 16px; border: 2px solid %1; "
        "  border-radius: 9px; background-color: #FFFFFF; image: none; }"
        "QCheckBox::indicator:hover { border-color: %2; }"
        "QCheckBox::indicator:checked { "
        "  background-color: %2; border-color: %2; image: none; }"
    ).arg(borderColor, stepColor));
    hbox->addWidget(cb);

    frame->setProperty("entryIndex", entryIdx);
    frame->setProperty("stepColor", stepColor);

    return frame;
}

// ========== 更新卡片样式 ==========

void StepConfigWidget::updateCardStyle(QFrame* frame, bool enabled)
{
    int entryIdx = entryIndexForFrame(frame);
    QString stepColor = QString::fromUtf8(kSteps[entryIdx].color);
    QString borderColor = enabled ? stepColor : "#CBD5E1";
    QString borderWidth = enabled ? "2px" : "1px";

    // 更新背景色
    QPalette pal = frame->palette();
    pal.setColor(QPalette::Window, enabled ? QColor("#FFFFFF") : QColor("#F1F5F9"));
    frame->setPalette(pal);

    // 更新边框
    frame->setStyleSheet(QString(
        "QFrame#stepCard { border: %1 solid %2; border-radius: 8px; }"
        "QFrame#stepCard:hover { border-color: %3; }"
    ).arg(borderWidth, borderColor, stepColor));

    // 更新编号圆圈颜色
    if (auto* numLabel = frame->findChild<QLabel*>("numberLabel")) {
        numLabel->setStyleSheet(QString(
            "QLabel { background-color: %1; color: white; border-radius: 11px; "
            "font-weight: bold; font-size: 10px; }"
        ).arg(enabled ? stepColor : "#94A3B8"));
    }

    // 更新图标颜色
    if (auto* iconLabel = frame->findChild<QLabel*>("iconLabel")) {
        iconLabel->setStyleSheet(QString(
            "QLabel { background-color: %1; color: white; border-radius: 3px; "
            "font-size: 8px; font-weight: bold; }"
        ).arg(enabled ? stepColor : "#94A3B8"));
    }

    // 更新名称颜色
    if (auto* nameLabel = frame->findChild<QLabel*>("nameLabel")) {
        nameLabel->setStyleSheet(QString(
            "QLabel { font-weight: bold; font-size: 11px; color: %1; background: transparent; }"
        ).arg(enabled ? "#1E293B" : "#475569"));
    }

    // 更新复选框颜色
    if (auto* cb = frame->findChild<QCheckBox*>()) {
        cb->setToolTip(enabled ? "已启用" : "点击启用");
        cb->setStyleSheet(QString(
            "QCheckBox { spacing: 4px; background: transparent; }"
            "QCheckBox::indicator { "
            "  width: 16px; height: 16px; border: 2px solid %1; "
            "  border-radius: 9px; background-color: #FFFFFF; image: none; }"
            "QCheckBox::indicator:hover { border-color: %2; }"
            "QCheckBox::indicator:checked { "
            "  background-color: %2; border-color: %2; image: none; }"
        ).arg(borderColor, stepColor));
    }

    // 更新光标
    frame->setCursor(enabled ? Qt::OpenHandCursor : Qt::ArrowCursor);
}

// ========== 重建步骤项 ==========

void StepConfigWidget::rebuildStepItems()
{
    if (!m_pipelineManager) return;

    const auto& config = m_pipelineManager->config();

    // 保存用户拖拽顺序（在清理之前）
    QList<int> userDragOrder;
    for (auto* f : m_selectedFrames) {
        userDragOrder.append(entryIndexForFrame(f));
    }

    // 清理旧帧
    m_dragSource = nullptr;
    for (auto* f : m_availableFrames) {
        f->removeEventFilter(this);
        f->deleteLater();
    }
    for (auto* f : m_selectedFrames) {
        f->removeEventFilter(this);
        f->deleteLater();
    }
    m_availableFrames.clear();
    m_selectedFrames.clear();

    // 清空待选区 layout
    while (m_availableOuterLayout->count() > 0) {
        auto* item = m_availableOuterLayout->takeAt(0);
        delete item;
    }
    // 清空确认区 layout
    while (m_selectedOuterLayout->count() > 0) {
        auto* item = m_selectedOuterLayout->takeAt(0);
        delete item;
    }

    // 分离 alwaysOn 和可排序步骤
    QList<int> normalEntries;
    int alwaysOnEntry = -1;
    for (int i = 0; i < kStepCount; ++i) {
        if (kSteps[i].alwaysOn) {
            alwaysOnEntry = i;
        } else {
            normalEntries.append(i);
        }
    }

    // 构建用户拖拽顺序的查找表
    QMap<int, int> userOrderMap;
    for (int i = 0; i < userDragOrder.size(); ++i) {
        userOrderMap[userDragOrder[i]] = i;
    }

    // 排序：优先使用用户拖拽顺序，回退到 stepOrder
    std::sort(normalEntries.begin(), normalEntries.end(),
              [&](int a, int b) {
        bool aInUser = userOrderMap.contains(a);
        bool bInUser = userOrderMap.contains(b);
        if (aInUser && bInUser) return userOrderMap[a] < userOrderMap[b];
        if (aInUser) return true;
        if (bInUser) return false;
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

    // 判断每个步骤是否启用
    auto isStepEnabled = [&](int entryIdx) -> bool {
        for (int idx : kSteps[entryIdx].backendIndices) {
            if (idx >= 0 && m_pipelineManager->isStepEnabled(idx)) return true;
        }
        if (kSteps[entryIdx].backendIndices.size() == 1
            && kSteps[entryIdx].backendIndices[0] < 0
            && strcmp(kSteps[entryIdx].displayName, "目标检测") == 0) {
            return config.enableObjectDetection;
        }
        return false;
    };

    // 创建可排序步骤卡片，按启用状态分到两个区域
    for (int entryIdx : normalEntries) {
        bool enabled = isStepEnabled(entryIdx);
        auto* frame = createStepCard(entryIdx, enabled, kSteps[entryIdx]);

        if (enabled) {
            frame->setCursor(Qt::OpenHandCursor);
            // 已选步骤卡片：固定高度，宽度铺满
            frame->setMinimumWidth(0);
            frame->setMaximumWidth(QWIDGETSIZE_MAX);
            frame->setFixedHeight(40);
            m_selectedFrames.append(frame);
        } else {
            if (auto* h = frame->findChild<QLabel*>("dragHandle")) h->hide();
            if (auto* n = frame->findChild<QLabel*>("numberLabel")) n->hide();
            m_availableFrames.append(frame);
        }
    }

    // 构建待选区网格（2列）
    QGridLayout* availGrid = new QGridLayout();
    availGrid->setSpacing(6);
    availGrid->setContentsMargins(0, 0, 0, 0);
    for (int i = 0; i < m_availableFrames.size(); ++i) {
        availGrid->addWidget(m_availableFrames[i], i / 2, i % 2);
    }
    m_availableOuterLayout->addLayout(availGrid);

    // 构建确认区（单列）
    m_selectedOuterLayout->addWidget(m_emptyHintLabel);
    for (auto* frame : m_selectedFrames) {
        m_selectedOuterLayout->addWidget(frame);
    }

    m_selectedOuterLayout->addStretch();

    // 注册确认区卡片的拖拽事件
    for (auto* frame : m_selectedFrames) {
        if (kSteps[entryIndexForFrame(frame)].alwaysOn) continue;
        frame->installEventFilter(this);
    }

    // 连接复选框信号（统一处理：勾选→确认区，取消→待选区）
    auto allFrames = m_availableFrames + m_selectedFrames;
    for (auto* frame : allFrames) {
        int entryIdx = entryIndexForFrame(frame);
        if (kSteps[entryIdx].alwaysOn) continue;
        auto* cb = frame->findChild<QCheckBox*>();
        if (cb) {
            connect(cb, &QCheckBox::toggled, this, [this, frame](bool checked) {
                if (checked && m_availableFrames.contains(frame)) {
                    moveStepToSelected(frame);
                } else if (!checked && m_selectedFrames.contains(frame)) {
                    moveStepToAvailable(frame);
                }
            });
        }
    }

    updateSelectedNumbers();
    updateAvailableAreaHeight();
    updatePipelinePreview();
}

// ========== 流程预览更新 ==========

void StepConfigWidget::updatePipelinePreview()
{
    if (!m_previewStepsLabel) return;

    struct StepInfo { QString icon; QString name; QString color; };
    QList<StepInfo> enabledSteps;

    for (auto* frame : m_selectedFrames) {
        int entryIdx = entryIndexForFrame(frame);
        if (entryIdx < 0) continue;
        auto* cb = frame->findChild<QCheckBox*>();
        if (!cb || !cb->isChecked()) continue;

        enabledSteps.append({
            QString::fromUtf8(kSteps[entryIdx].icon),
            QString::fromUtf8(kSteps[entryIdx].displayName),
            QString::fromUtf8(kSteps[entryIdx].color)
        });
    }

    if (enabledSteps.isEmpty()) {
        m_previewStepsLabel->setText("未启用任何步骤");
        m_previewStepsLabel->setStyleSheet("QLabel { color: #94A3B8; font-size: 11px; background: transparent; }");
    } else {
        const int perRow = 3;
        QString html;
        for (int i = 0; i < enabledSteps.size(); ++i) {
            if (i > 0) {
                if (i % perRow == 0) {
                    html += "<br><br>";
                } else {
                    html += "<span style='color: #94A3B8; font-size: 11px; margin: 0 2px;'> → </span>";
                }
            }
            const auto& s = enabledSteps[i];
            html += QString("<span style='"
                "background-color: %1; color: white; "
                "padding: 2px 8px; border-radius: 10px; "
                "font-size: 11px; font-weight: bold; "
                "white-space: nowrap;'>"
                "%2 %3</span>")
                .arg(s.color, s.icon, s.name);
        }
        m_previewStepsLabel->setText(html);
        m_previewStepsLabel->setStyleSheet("QLabel { background: transparent; }");
    }
}

// ========== 应用 ==========

void StepConfigWidget::onApplyClicked()
{
    if (!m_pipelineManager) return;

    std::array<int, PipelineConfig::STEP_COUNT> newOrder{};
    QSet<int> placed;
    int pos = 0;

    for (auto* frame : m_selectedFrames) {
        int entryIdx = entryIndexForFrame(frame);
        if (entryIdx < 0) continue;

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

        if (checked && kSteps[entryIdx].backendIndices.contains(2)) {
            m_pipelineManager->updateConfig([&](PipelineConfig& cfg) {
                if (cfg.colorFilter.mode == ImageFilterMode::None) {
                    cfg.colorFilter.mode = ImageFilterMode::Gray;
                }
            });
        }

        if (kSteps[entryIdx].backendIndices.size() == 1
            && kSteps[entryIdx].backendIndices[0] < 0
            && strcmp(kSteps[entryIdx].displayName, "目标检测") == 0) {
            m_pipelineManager->updateConfig([checked](PipelineConfig& cfg) {
                cfg.enableObjectDetection = checked;
            });
        }
    }

    for (int i = 0; i < PipelineConfig::STEP_COUNT; ++i) {
        if (!placed.contains(i)) {
            newOrder[pos++] = i;
        }
    }

    m_pipelineManager->updateConfig([&](PipelineConfig& cfg) {
        for (int i = 0; i < PipelineConfig::STEP_COUNT; ++i) {
            cfg.stepOrder[i] = newOrder[i];
        }
    });

    m_pipelineManager->rebuildPipeline();
    emit tabsNeeded(collectEnabledTabNames());
    applyTabVisibility();
    if (m_onExecutePipeline) m_onExecutePipeline();
    updatePipelinePreview();

    // [FIX] 保存配置到当前图片（per-image 隔离）
    if (m_roiManager) {
        QString imageId = m_roiManager->getCurrentImageId();
        if (!imageId.isEmpty()) {
            m_roiManager->saveImagePipelineConfig(imageId, m_pipelineManager->getConfigSnapshot());
        }
    }
}

// ========== Tab 管理 ==========

QStringList StepConfigWidget::collectEnabledTabNames()
{
    QSet<QString> checked;
    for (auto* frame : m_selectedFrames) {
        int entryIdx = entryIndexForFrame(frame);
        if (entryIdx < 0) continue;
        auto* cb = frame->findChild<QCheckBox*>();
        if (!cb || !cb->isChecked()) continue;

        for (const auto& name : kSteps[entryIdx].tabNames) {
            checked.insert(name);
        }
    }
    checked.insert(QStringLiteral("步骤"));

    QStringList result;
    for (auto* frame : m_selectedFrames) {
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

// ========== IConfigurableTab 接口实现 ==========

void StepConfigWidget::saveToConfig(PipelineConfig& config) const
{
    // 步骤配置已经在 onApplyClicked() 中直接写入 PipelineManager
    // 这里不需要额外操作
    Q_UNUSED(config);
}

void StepConfigWidget::loadFromConfig(const PipelineConfig& config)
{
    Q_UNUSED(config);
    // 当配置变化时，重新构建步骤项以反映新的 stepEnabled 状态
    rebuildStepItems();
    updatePipelinePreview();
    applyTabVisibility();
}
