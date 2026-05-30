#include "ocr_tab_widget.h"
#include "config/pipeline_config.h"
#include "controllers/roi_ui_controller.h"
#include "core/roi_manager.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QFormLayout>
#include <QHeaderView>
#include <QClipboard>
#include <QApplication>
#include <QMessageBox>

OcrTabWidget::OcrTabWidget(PipelineManager* pipelineManager, QWidget* parent)
    : QWidget(parent)
    , m_pipelineManager(pipelineManager)
{
    setupUI();
}

OcrTabWidget::~OcrTabWidget()
{
}

void OcrTabWidget::setupUI()
{
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(8, 8, 8, 8);

    // 排列模式
    auto* langGroup = new QGroupBox("排列模式");
    auto* langLayout = new QFormLayout(langGroup);

    m_pageModeCombo = new QComboBox();
    m_pageModeCombo->addItem("单行文字", 1);
    m_pageModeCombo->addItem("多行文字", 2);
    langLayout->addRow("文字排列:", m_pageModeCombo);

    mainLayout->addWidget(langGroup);

    // 判定参数
    auto* judgeGroup = new QGroupBox("判定参数");
    auto* judgeLayout = new QFormLayout(judgeGroup);

    m_expectedTextLineEdit = new QLineEdit();
    m_expectedTextLineEdit->setPlaceholderText("留空则不检查文本内容");
    judgeLayout->addRow("期望文本:", m_expectedTextLineEdit);

    m_matchExactCheckBox = new QCheckBox("精确匹配");
    m_matchExactCheckBox->setToolTip("勾选后必须完全匹配，不勾选则包含即可");
    judgeLayout->addRow("", m_matchExactCheckBox);

    mainLayout->addWidget(judgeGroup);

    // 操作提示
    m_hintLabel = new QLabel(QString::fromUtf8("提示：请先配置ROI区域，然后点击[识别]按钮进行文字识别"));
    m_hintLabel->setStyleSheet("color: #666; padding: 5px; background: #f5f5f5; border-radius: 3px;");
    m_hintLabel->setWordWrap(true);
    mainLayout->addWidget(m_hintLabel);

    // 操作按钮
    auto* btnLayout = new QHBoxLayout();
    
    m_recognizeBtn = new QPushButton("识别");
    m_recognizeBtn->setStyleSheet("QPushButton { font-weight: bold; padding: 8px 20px; }");
    connect(m_recognizeBtn, &QPushButton::clicked, this, &OcrTabWidget::onRecognizeClicked);
    btnLayout->addWidget(m_recognizeBtn);

    m_copyBtn = new QPushButton("复制结果");
    m_copyBtn->setEnabled(false);
    connect(m_copyBtn, &QPushButton::clicked, this, &OcrTabWidget::onCopyResultClicked);
    btnLayout->addWidget(m_copyBtn);

    m_resetBtn = new QPushButton("重置");
    connect(m_resetBtn, &QPushButton::clicked, this, &OcrTabWidget::onResetClicked);
    btnLayout->addWidget(m_resetBtn);

    mainLayout->addLayout(btnLayout);

    // ========== 识别结果显示 ==========
    auto* resultGroup = new QGroupBox("识别结果");
    auto* resultLayout = new QVBoxLayout(resultGroup);

    // 状态标签
    m_statusLabel = new QLabel("等待识别...");
    m_statusLabel->setStyleSheet("font-weight: bold; color: gray;");
    resultLayout->addWidget(m_statusLabel);

    // 识别文本显示
    auto* textLabel = new QLabel("识别文本：");
    resultLayout->addWidget(textLabel);
    m_resultTextEdit = new QTextEdit();
    m_resultTextEdit->setReadOnly(true);
    m_resultTextEdit->setMaximumHeight(100);
    m_resultTextEdit->setPlaceholderText("识别结果将显示在这里...");
    resultLayout->addWidget(m_resultTextEdit);

    // 区域表格
    auto* tableLabel = new QLabel("识别区域：");
    resultLayout->addWidget(tableLabel);
    m_regionsTable = new QTableWidget();
    m_regionsTable->setColumnCount(5);
    m_regionsTable->setHorizontalHeaderLabels({"文字", "X", "Y", "宽度", "高度"});
    m_regionsTable->horizontalHeader()->setStretchLastSection(true);
    m_regionsTable->setEditTriggers(QTableWidget::NoEditTriggers);
    m_regionsTable->setSelectionBehavior(QTableWidget::SelectRows);
    m_regionsTable->setMaximumHeight(120);
    resultLayout->addWidget(m_regionsTable);

    mainLayout->addWidget(resultGroup);
    mainLayout->addStretch();

    // 连接信号（切换排列模式时同步配置，不自动触发识别）
    connect(m_pageModeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this]() { syncConfigToPipeline(); });
}

void OcrTabWidget::onRecognizeClicked()
{
    // 标记为手动触发，updateFromPipeline 仅在此标记下才更新 UI
    m_manualOcrTrigger = true;

    // 同步配置到Pipeline
    syncConfigToPipeline();

    // 触发Pipeline执行
    emit processRequested();

    // 更新状态
    m_statusLabel->setText("正在识别...");
    m_statusLabel->setStyleSheet("font-weight: bold; color: blue;");
}

void OcrTabWidget::onCopyResultClicked()
{
    QString text = m_resultTextEdit->toPlainText();
    if (text.isEmpty()) {
        QMessageBox::information(this, "提示", "没有可复制的内容");
        return;
    }
    
    QApplication::clipboard()->setText(text);
    m_statusLabel->setText("已复制到剪贴板");
    m_statusLabel->setStyleSheet("font-weight: bold; color: green;");
}

void OcrTabWidget::onResetClicked()
{
    clearResults();
    m_pageModeCombo->setCurrentIndex(0);

    syncConfigToPipeline();
}

void OcrTabWidget::syncConfigToPipeline()
{
    if (!m_pipelineManager) return;

    m_pipelineManager->updateConfig([&](PipelineConfig& cfg) {
        cfg.ocr.pageMode = m_pageModeCombo->currentData().toInt();
    });
}

void OcrTabWidget::clearResults()
{
    m_manualOcrTrigger = false;
    m_resultTextEdit->clear();
    m_regionsTable->setRowCount(0);
    m_statusLabel->setText("等待识别...");
    m_statusLabel->setStyleSheet("font-weight: bold; color: gray;");
    m_copyBtn->setEnabled(false);
}

void OcrTabWidget::saveToConfig(PipelineConfig& config) const
{
    config.ocr.pageMode = m_pageModeCombo->currentData().toInt();
}

void OcrTabWidget::loadFromConfig(const PipelineConfig& config)
{
    const auto& cfg = config.ocr;

    int pageModeIndex = m_pageModeCombo->findData(cfg.pageMode);
    if (pageModeIndex >= 0) {
        m_pageModeCombo->setCurrentIndex(pageModeIndex);
    }
}

void OcrTabWidget::connectSignals(const SignalContext& ctx,
                                  std::function<void()> onExecutePipeline,
                                  std::function<void()> onConfigSaved)
{
    Q_UNUSED(onConfigSaved);

    // 图片切换时清空上次识别结果，防止旧图结果自动显示在新图Tab中
    if (ctx.roiManager) {
        connect(ctx.roiManager, &RoiManager::currentImageChanged, this, &OcrTabWidget::clearResults);
    }

    connect(this, &OcrTabWidget::processRequested, this, [onExecutePipeline]() {
        if (onExecutePipeline) onExecutePipeline();
    });

    // 判定参数变化时同步到DetectionItem.config
    connect(m_expectedTextLineEdit, &QLineEdit::editingFinished, this, [this, ctx]() {
        if (ctx.roiCtrl) {
            ctx.roiCtrl->updateOcrDetectionConfig(
                m_expectedTextLineEdit->text(),
                m_matchExactCheckBox->isChecked()
            );
        }
    });
    connect(m_matchExactCheckBox, &QCheckBox::toggled, this, [this, ctx](bool checked) {
        Q_UNUSED(checked);
        if (ctx.roiCtrl) {
            ctx.roiCtrl->updateOcrDetectionConfig(
                m_expectedTextLineEdit->text(),
                m_matchExactCheckBox->isChecked()
            );
        }
    });
}

void OcrTabWidget::updateFromPipeline(const PipelineContext& ctx)
{
    // 非手动触发时，忽略管线执行的结果（防止其他检测类型清空OCR结果）
    // 但允许外部清空通知（config为空表示图片切换等场景）
    if (!m_manualOcrTrigger && ctx.config != nullptr) {
        return;
    }

    if (m_manualOcrTrigger) {
        m_manualOcrTrigger = false;
    }

    // 如果是清空操作（空的ocrText），直接执行
    if (ctx.ocrText.isEmpty() && ctx.ocrRegions.isEmpty()) {
        m_resultTextEdit->clear();
        updateRegionsTable(QVector<OcrRegion>());
        m_statusLabel->setText("等待识别...");
        m_statusLabel->setStyleSheet("font-weight: bold; color: gray;");
        m_copyBtn->setEnabled(false);
        return;
    }

    // 更新识别文本
    m_resultTextEdit->setText(ctx.ocrText);

    // 更新区域表格
    updateRegionsTable(ctx.ocrRegions);

    // 更新状态
    if (ctx.ocrText.isEmpty()) {
        m_statusLabel->setText("未识别到文字");
        m_statusLabel->setStyleSheet("font-weight: bold; color: orange;");
        m_copyBtn->setEnabled(false);
    } else {
        int count = ctx.ocrRegions.size();
        int charCount = ctx.ocrText.length();
        m_statusLabel->setText(QString("识别完成: %1 个区域, %2 个字符").arg(count).arg(charCount));
        m_statusLabel->setStyleSheet("font-weight: bold; color: green;");
        m_copyBtn->setEnabled(true);
    }
}

void OcrTabWidget::updateRegionsTable(const QVector<OcrRegion>& regions)
{
    m_regionsTable->setRowCount(regions.size());

    for (int i = 0; i < regions.size(); ++i) {
        const auto& region = regions[i];

        auto* textItem = new QTableWidgetItem(region.text);
        auto* xItem = new QTableWidgetItem(QString::number(region.x));
        auto* yItem = new QTableWidgetItem(QString::number(region.y));
        auto* wItem = new QTableWidgetItem(QString::number(region.width));
        auto* hItem = new QTableWidgetItem(QString::number(region.height));

        textItem->setToolTip(QString("置信度: %1%").arg(region.confidence * 100, 0, 'f', 1));

        m_regionsTable->setItem(i, 0, textItem);
        m_regionsTable->setItem(i, 1, xItem);
        m_regionsTable->setItem(i, 2, yItem);
        m_regionsTable->setItem(i, 3, wItem);
        m_regionsTable->setItem(i, 4, hItem);
    }
}
