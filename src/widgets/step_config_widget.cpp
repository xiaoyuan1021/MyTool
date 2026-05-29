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
#include <QFrame>
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
//   icon            步骤图标（Unicode符号）
//   description     步骤描述（tooltip显示）
//   color           步骤主题色
// ============================================================
const StepConfigWidget::StepEntry StepConfigWidget::kSteps[] = {
    {"颜色通道",      {"图像"},        {0},           false, "CH", "选择颜色通道：Gray/RGB/HSV", "#4FACFE"},
    {"图像增强",      {"增强"},        {1},           false, "EN", "亮度/对比度/Gamma/锐化", "#7C4DFF"},
    // 统一过滤（灰度/RGB/HSV）
    {"过滤",          {"过滤"},        {2},           false, "FT", "灰度/RGB/HSV范围过滤", "#00BFA5"},
    {"算法处理",      {"处理"},        {3},           false, "AL", "形态学操作：开闭运算/膨胀腐蚀", "#FF6D00"},
    {"形状筛选",      {"提取"},        {4},           false, "SF", "面积/圆度/凸性/矩形度筛选", "#E91E63"},
    // 直线检测（含参考线匹配）
    {"直线检测",      {"直线"},        {5},           false, "LD", "HoughP/LSD/EDline直线检测", "#2196F3"},
    // 模板匹配——无对应后端步骤，仅控制 Tab 显隐
    {"模板匹配",      {"补正"},        {-1},          false, "TM", "模板匹配定位与对齐", "#9C27B0"},
    {"条码识别",      {"条码"},        {6},           false, "BC", "QR Code/条形码识别", "#4CAF50"},
    // 滤波去噪——对应 StepImageFilter(7)
    {"滤波去噪",      {"滤波去噪"},    {7},           false, "FN", "高斯/中值/双边滤波去噪", "#00BCD4"},
    // 文字识别——对应 StepOcrRecognition(8)
    {"文字识别",      {"文字识别"},    {8},           false, "OC", "Tesseract OCR文字识别", "#FF5722"},
    // 目标检测——无对应后端步骤，通过 PipelineConfig::enableObjectDetection 控制
    {"目标检测",      {"目标检测"},    {-1},          false, "DL", "YOLO深度学习目标检测", "#607D8B"},

    // 步骤 Tab 自身始终显示（单独放在列表下方）
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
    m_onExecutePipeline = std::move(onExecutePipeline);

    if (ctx.roiCtrl) {
        connect(ctx.roiCtrl, &RoiUiController::roiPipelineConfigChanged,
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

            // 拖拽完成后实时更新预览
            updatePipelinePreview();
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
    mainLayout->setSpacing(12);
    mainLayout->setContentsMargins(8, 8, 8, 8);

    // 标题区域
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

    // 说明文字
    auto* descLabel = new QLabel(
        QStringLiteral("拖拽调整步骤顺序，勾选启用步骤，点击「应用」生效"),
        this);
    descLabel->setStyleSheet(
        "QLabel { color: #6B7280; font-size: 11px; padding: 0 0 0 36px; background: transparent; }");
    descLabel->setWordWrap(true);
    mainLayout->addWidget(descLabel);

    // 流程预览区域
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
    m_previewStepsLabel->setStyleSheet("QLabel { background: transparent; }");
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

    // 可拖拽排序的步骤容器
    m_scrollArea = new QScrollArea(this);
    m_scrollArea->setWidgetResizable(true);
    m_scrollArea->setFrameShape(QFrame::NoFrame);
    m_scrollArea->setStyleSheet(
        "QScrollArea { background: transparent; border: none; }"
        "QScrollBar:vertical { background-color: transparent; width: 6px; }"
        "QScrollBar::handle:vertical { background-color: #C8D4E0; border-radius: 3px; min-height: 24px; }"
        "QScrollBar::handle:vertical:hover { background-color: #4FACFE; }");

    m_stepContainer = new QWidget();
    m_stepContainer->setAcceptDrops(true);
    m_stepContainer->installEventFilter(this);
    m_stepContainer->setAutoFillBackground(true);
    QPalette containerPal;
    containerPal.setColor(QPalette::Window, Qt::transparent);
    m_stepContainer->setPalette(containerPal);
    
    m_stepLayout = new QVBoxLayout(m_stepContainer);
    m_stepLayout->setSpacing(0);
    m_stepLayout->setContentsMargins(0, 0, 0, 0);

    m_scrollArea->setWidget(m_stepContainer);
    mainLayout->addWidget(m_scrollArea, 1);

    // 应用按钮区域
    auto* btnLayout = new QHBoxLayout();
    btnLayout->setSpacing(8);
    
    m_applyBtn = new QPushButton(QStringLiteral("应用配置"), this);
    m_applyBtn->setMinimumHeight(36);
    m_applyBtn->setAutoFillBackground(true);
    QPalette btnPal;
    btnPal.setColor(QPalette::Button, QColor("#4FACFE"));
    btnPal.setColor(QPalette::ButtonText, Qt::white);
    m_applyBtn->setPalette(btnPal);
    m_applyBtn->setStyleSheet(
        "QPushButton { "
        "  border: none; border-radius: 6px;"
        "  font-weight: bold; font-size: 13px; padding: 8px 16px; "
        "  background-color: #4FACFE; color: white; "
        "}"
        "QPushButton:hover { background-color: #3D9AF0; }"
        "QPushButton:pressed { background-color: #2A85D8; }"
        "QPushButton:disabled { background-color: #B8D4E8; }");
    btnLayout->addWidget(m_applyBtn);

    mainLayout->addLayout(btnLayout);

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
    int stepNum = 1;
    for (int entryIdx : sortableEntries) {
        // 步骤之间的连接线（除了第一个）
        if (stepNum > 1) {
            auto* connector = new QLabel("|", m_stepContainer);
            connector->setAlignment(Qt::AlignCenter);
            connector->setStyleSheet(
                "QLabel { color: #CBD5E1; font-size: 16px; background: transparent; }");
            connector->setFixedHeight(10);
            m_stepLayout->addWidget(connector);
        }

        auto* frame = new QFrame();
        frame->setObjectName("stepCard");
        frame->setCursor(Qt::OpenHandCursor);
        frame->setMinimumHeight(56);
        
        // 确定步骤状态颜色
        bool anyEnabled = false;
        for (int idx : kSteps[entryIdx].backendIndices) {
            if (idx >= 0 && m_pipelineManager->isStepEnabled(idx)) {
                anyEnabled = true;
                break;
            }
        }
        if (!anyEnabled && kSteps[entryIdx].backendIndices.size() == 1
            && kSteps[entryIdx].backendIndices[0] < 0
            && strcmp(kSteps[entryIdx].displayName, "目标检测") == 0) {
            anyEnabled = config.enableObjectDetection;
        }

        QString stepColor = QString::fromUtf8(kSteps[entryIdx].color);

        frame->setAutoFillBackground(true);
        QPalette pal = frame->palette();
        pal.setColor(QPalette::Window, anyEnabled ? QColor("#FFFFFF") : QColor("#F1F5F9"));
        frame->setPalette(pal);

        QString borderColor = anyEnabled ? stepColor : "#CBD5E1";
        QString borderWidth = anyEnabled ? "2px" : "1px";
        frame->setStyleSheet(QString(
            "QFrame#stepCard { "
            "  border: %1 solid %2; "
            "  border-radius: 8px; "
            "}"
            "QFrame#stepCard:hover { "
            "  border-color: %3; "
            "}"
        ).arg(borderWidth, borderColor, stepColor));

        auto* hbox = new QHBoxLayout(frame);
        hbox->setContentsMargins(12, 10, 12, 10);
        hbox->setSpacing(8);

        // 拖拽手柄
        auto* dragHandle = new QLabel("::", frame);
        dragHandle->setStyleSheet(
            "QLabel { color: #94A3B8; font-size: 16px; padding: 0 4px; background: transparent; }");
        dragHandle->setCursor(Qt::OpenHandCursor);
        dragHandle->setToolTip("拖拽调整顺序");
        hbox->addWidget(dragHandle);

        // 步骤编号圆圈
        auto* numberLabel = new QLabel(QString::number(stepNum), frame);
        numberLabel->setAlignment(Qt::AlignCenter);
        numberLabel->setFixedSize(28, 28);
        numberLabel->setStyleSheet(QString(
            "QLabel { "
            "  background-color: %1; color: white; "
            "  border-radius: 14px; "
            "  font-weight: bold; font-size: 12px; "
            "}"
        ).arg(anyEnabled ? stepColor : "#94A3B8"));
        hbox->addWidget(numberLabel);

        // 步骤图标（缩写文字 + 主题色背景）
        auto* iconLabel = new QLabel(QString::fromUtf8(kSteps[entryIdx].icon), frame);
        iconLabel->setAlignment(Qt::AlignCenter);
        iconLabel->setFixedSize(28, 20);
        iconLabel->setStyleSheet(QString(
            "QLabel { "
            "  background-color: %1; color: white; "
            "  border-radius: 4px; "
            "  font-size: 10px; font-weight: bold; "
            "}"
        ).arg(anyEnabled ? stepColor : "#94A3B8"));
        hbox->addWidget(iconLabel);

        // 步骤名称和描述
        auto* textLayout = new QVBoxLayout();
        textLayout->setSpacing(2);
        textLayout->setContentsMargins(0, 0, 0, 0);

        auto* nameLabel = new QLabel(
            QString::fromUtf8(kSteps[entryIdx].displayName), frame);
        nameLabel->setStyleSheet(QString(
            "QLabel { font-weight: bold; font-size: 13px; color: %1; background: transparent; }"
        ).arg(anyEnabled ? "#1E293B" : "#475569"));
        textLayout->addWidget(nameLabel);

        auto* descLabel = new QLabel(
            QString::fromUtf8(kSteps[entryIdx].description), frame);
        descLabel->setStyleSheet(
            "QLabel { font-size: 10px; color: #64748B; background: transparent; }");
        textLayout->addWidget(descLabel);

        hbox->addLayout(textLayout, 1);

        // 复选框
        auto* cb = new QCheckBox(frame);
        cb->setChecked(anyEnabled);
        cb->setToolTip(anyEnabled ? "已启用" : "点击启用");
        cb->setStyleSheet(QString(
            "QCheckBox { spacing: 6px; background: transparent; }"
            "QCheckBox::indicator { "
            "  width: 22px; height: 22px; "
            "  border: 2px solid %1; "
            "  border-radius: 11px; "
            "  background-color: #FFFFFF; "
            "  image: none; "
            "}"
            "QCheckBox::indicator:hover { "
            "  border-color: %2; "
            "}"
            "QCheckBox::indicator:checked { "
            "  background-color: %2; "
            "  border-color: %2; "
            "  image: none; "
            "}"
        ).arg(borderColor, stepColor));
        hbox->addWidget(cb);

        // 勾选状态改变时实时更新预览
        connect(cb, &QCheckBox::toggled, this, &StepConfigWidget::updatePipelinePreview);

        frame->setProperty("entryIndex", entryIdx);
        frame->setProperty("stepColor", stepColor);
        frame->installEventFilter(this);

        m_stepLayout->addWidget(frame);
        m_stepFrames.append(frame);
        stepNum++;
    }

    // 添加弹性空间
    m_stepLayout->addStretch();

    // 更新流程预览
    updatePipelinePreview();
}

// ========== 流程预览更新 ==========

void StepConfigWidget::updatePipelinePreview()
{
    if (!m_previewStepsLabel || m_stepFrames.isEmpty()) return;

    // 收集已启用的步骤
    struct StepInfo { QString icon; QString name; QString color; };
    QList<StepInfo> enabledSteps;
    for (auto* frame : m_stepFrames) {
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

    // 用HTML构建彩色徽章预览
    if (enabledSteps.isEmpty()) {
        m_previewStepsLabel->setText("未启用任何步骤");
        m_previewStepsLabel->setStyleSheet("QLabel { color: #94A3B8; font-size: 11px; background: transparent; }");
    } else {
        QString html;
        for (int i = 0; i < enabledSteps.size(); ++i) {
            if (i > 0) {
                html += "<span style='color: #94A3B8; font-size: 11px; margin: 0 2px;'> → </span>";
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
            m_pipelineManager->updateConfig([&](PipelineConfig& cfg) {
                if (cfg.colorFilter.mode == ImageFilterMode::None) {
                    cfg.colorFilter.mode = ImageFilterMode::Gray;
                }
            });
        }

        // 虚拟步骤：持久化到 PipelineConfig 独立字段
        if (kSteps[entryIdx].backendIndices.size() == 1
            && kSteps[entryIdx].backendIndices[0] < 0
            && strcmp(kSteps[entryIdx].displayName, "目标检测") == 0) {
            m_pipelineManager->updateConfig([checked](PipelineConfig& cfg) {
                cfg.enableObjectDetection = checked;
            });
        }
    }

    // 补全未出现的索引
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

    // ---- 2) 重建 Pipeline ----
    m_pipelineManager->rebuildPipeline();

    // ---- 3) 按用户排序创建 Tab ----
    emit tabsNeeded(collectEnabledTabNames());

    // ---- 4) 动态显示/隐藏 ----
    applyTabVisibility();

    // ---- 5) 触发刷新 ----
    emit stepConfigChanged();
    if (m_onExecutePipeline) m_onExecutePipeline();

    // ---- 6) 更新流程预览 ----
    updatePipelinePreview();
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
