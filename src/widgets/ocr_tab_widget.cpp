#include "ocr_tab_widget.h"
#include "config/pipeline_config.h"
#include "controllers/roi_ui_controller.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QPushButton>
#include <QFormLayout>
#include <QHeaderView>

OcrTabWidget::OcrTabWidget(PipelineManager* pipelineManager, QWidget* parent)
    : QWidget(parent)
    , m_pipelineManager(pipelineManager)
    , m_debounceTimer(new QTimer(this))
{
    setupUI();
    m_debounceTimer->setInterval(100);
    connect(m_debounceTimer, &QTimer::timeout, this, [this]() {
        emit processRequested();
        m_debounceTimer->stop();
    });
}

OcrTabWidget::~OcrTabWidget()
{
}

void OcrTabWidget::setupUI()
{
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(8, 8, 8, 8);

    // 语言选择
    auto* langGroup = new QGroupBox("识别语言");
    auto* langLayout = new QFormLayout(langGroup);
    m_languageCombo = new QComboBox();
    m_languageCombo->addItem("中文+英文", "chi_sim+eng");
    m_languageCombo->addItem("仅中文", "chi_sim");
    m_languageCombo->addItem("仅英文", "eng");
    langLayout->addRow("语言:", m_languageCombo);
    mainLayout->addWidget(langGroup);

    // 字体大小范围
    auto* sizeGroup = new QGroupBox("字体大小范围");
    auto* sizeLayout = new QFormLayout(sizeGroup);

    m_minFontSizeSlider = new QSlider(Qt::Horizontal);
    m_minFontSizeSlider->setRange(1, 50);
    m_minFontSizeSlider->setValue(10);
    m_minFontSizeLabel = new QLabel("10");
    auto* minSizeLayout = new QHBoxLayout();
    minSizeLayout->addWidget(m_minFontSizeSlider);
    minSizeLayout->addWidget(m_minFontSizeLabel);
    sizeLayout->addRow("最小:", minSizeLayout);

    m_maxFontSizeSlider = new QSlider(Qt::Horizontal);
    m_maxFontSizeSlider->setRange(50, 500);
    m_maxFontSizeSlider->setValue(200);
    m_maxFontSizeLabel = new QLabel("200");
    auto* maxSizeLayout = new QHBoxLayout();
    maxSizeLayout->addWidget(m_maxFontSizeSlider);
    maxSizeLayout->addWidget(m_maxFontSizeLabel);
    sizeLayout->addRow("最大:", maxSizeLayout);

    mainLayout->addWidget(sizeGroup);

    // 预处理选项
    auto* preprocessGroup = new QGroupBox("预处理");
    auto* preprocessLayout = new QFormLayout(preprocessGroup);

    m_preprocessCheckBox = new QCheckBox("启用二值化预处理");
    m_preprocessCheckBox->setChecked(true);
    preprocessLayout->addRow(m_preprocessCheckBox);

    m_binaryThresholdSlider = new QSlider(Qt::Horizontal);
    m_binaryThresholdSlider->setRange(0, 255);
    m_binaryThresholdSlider->setValue(128);
    m_binaryThresholdLabel = new QLabel("128");
    auto* thresholdLayout = new QHBoxLayout();
    thresholdLayout->addWidget(m_binaryThresholdSlider);
    thresholdLayout->addWidget(m_binaryThresholdLabel);
    preprocessLayout->addRow("阈值:", thresholdLayout);

    mainLayout->addWidget(preprocessGroup);

    // 调试选项
    auto* debugGroup = new QGroupBox("调试");
    auto* debugLayout = new QFormLayout(debugGroup);
    m_detectOnlyCheckBox = new QCheckBox("仅检测不识别（调试用）");
    debugLayout->addRow(m_detectOnlyCheckBox);
    mainLayout->addWidget(debugGroup);

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
    m_resultTextEdit->setMaximumHeight(80);
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

    // 连接信号
    connect(m_languageCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &OcrTabWidget::onLanguageChanged);
    connect(m_minFontSizeSlider, &QSlider::valueChanged, this, [this](int v) {
        m_minFontSizeLabel->setText(QString::number(v));
        syncConfigToPipeline();
        m_debounceTimer->start();
    });
    connect(m_maxFontSizeSlider, &QSlider::valueChanged, this, [this](int v) {
        m_maxFontSizeLabel->setText(QString::number(v));
        syncConfigToPipeline();
        m_debounceTimer->start();
    });
    connect(m_preprocessCheckBox, &QCheckBox::toggled, this, &OcrTabWidget::onPreprocessToggled);
    connect(m_binaryThresholdSlider, &QSlider::valueChanged, this, [this](int v) {
        m_binaryThresholdLabel->setText(QString::number(v));
        syncConfigToPipeline();
        m_debounceTimer->start();
    });
    connect(m_detectOnlyCheckBox, &QCheckBox::toggled, this, [this](bool) {
        syncConfigToPipeline();
        m_debounceTimer->start();
    });

    // 重置按钮
    auto* btnLayout = new QHBoxLayout();
    auto* resetBtn = new QPushButton("重置参数");
    connect(resetBtn, &QPushButton::clicked, this, &OcrTabWidget::onResetClicked);
    btnLayout->addStretch();
    btnLayout->addWidget(resetBtn);
    mainLayout->addLayout(btnLayout);

    mainLayout->addStretch();
}

void OcrTabWidget::onLanguageChanged(int index)
{
    Q_UNUSED(index);
    syncConfigToPipeline();
    m_debounceTimer->start();
}

void OcrTabWidget::onPreprocessToggled(bool checked)
{
    m_binaryThresholdSlider->setEnabled(checked);
    syncConfigToPipeline();
    m_debounceTimer->start();
}

void OcrTabWidget::onResetClicked()
{
    m_languageCombo->setCurrentIndex(0);
    m_minFontSizeSlider->setValue(10);
    m_maxFontSizeSlider->setValue(200);
    m_preprocessCheckBox->setChecked(true);
    m_binaryThresholdSlider->setValue(128);
    m_detectOnlyCheckBox->setChecked(false);

    syncConfigToPipeline();
    m_debounceTimer->start();
}

void OcrTabWidget::syncConfigToPipeline()
{
    if (!m_pipelineManager) return;

    auto& cfg = m_pipelineManager->mutableConfig().ocr;
    cfg.language = m_languageCombo->currentData().toString();
    cfg.minFontSize = m_minFontSizeSlider->value();
    cfg.maxFontSize = m_maxFontSizeSlider->value();
    cfg.enablePreprocess = m_preprocessCheckBox->isChecked();
    cfg.binaryThreshold = m_binaryThresholdSlider->value();
    cfg.detectOnly = m_detectOnlyCheckBox->isChecked();

    emit ocrConfigChanged();
}

void OcrTabWidget::saveToConfig(PipelineConfig& config) const
{
    config.ocr.language = m_languageCombo->currentData().toString();
    config.ocr.minFontSize = m_minFontSizeSlider->value();
    config.ocr.maxFontSize = m_maxFontSizeSlider->value();
    config.ocr.enablePreprocess = m_preprocessCheckBox->isChecked();
    config.ocr.binaryThreshold = m_binaryThresholdSlider->value();
    config.ocr.detectOnly = m_detectOnlyCheckBox->isChecked();
}

void OcrTabWidget::loadFromConfig(const PipelineConfig& config)
{
    const auto& cfg = config.ocr;

    int langIndex = m_languageCombo->findData(cfg.language);
    if (langIndex >= 0) {
        m_languageCombo->setCurrentIndex(langIndex);
    }
    m_minFontSizeSlider->setValue(cfg.minFontSize);
    m_maxFontSizeSlider->setValue(cfg.maxFontSize);
    m_preprocessCheckBox->setChecked(cfg.enablePreprocess);
    m_binaryThresholdSlider->setValue(cfg.binaryThreshold);
    m_detectOnlyCheckBox->setChecked(cfg.detectOnly);
}

void OcrTabWidget::connectSignals(PipelineManager* pm, RoiManager* rm,
                                  ImageView* view, RoiUiController* roiCtrl,
                                  std::function<void()> requestRefresh,
                                  std::function<void()> processAndDisplay)
{
    Q_UNUSED(rm);
    Q_UNUSED(view);
    Q_UNUSED(roiCtrl);

    connect(this, &OcrTabWidget::processRequested, this, [processAndDisplay]() {
        if (processAndDisplay) processAndDisplay();
    });

    connect(this, &OcrTabWidget::ocrConfigChanged, this, [requestRefresh]() {
        if (requestRefresh) requestRefresh();
    });
}

void OcrTabWidget::updateFromPipeline(const PipelineContext& ctx)
{
    // 更新识别文本
    m_resultTextEdit->setText(ctx.ocrText);

    // 更新区域表格
    updateRegionsTable(ctx.ocrRegions);

    // 更新状态
    if (ctx.ocrText.isEmpty()) {
        m_statusLabel->setText("未识别到文字");
        m_statusLabel->setStyleSheet("font-weight: bold; color: orange;");
    } else {
        int count = ctx.ocrRegions.size();
        int charCount = ctx.ocrText.length();
        m_statusLabel->setText(QString("识别完成: %1 个区域, %2 个字符").arg(count).arg(charCount));
        m_statusLabel->setStyleSheet("font-weight: bold; color: green;");
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
