#include "image_filter_tab_widget.h"
#include "config/pipeline_config.h"
#include "controllers/roi_ui_controller.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QStackedWidget>

ImageFilterTabWidget::ImageFilterTabWidget(PipelineManager* pipelineManager, QWidget* parent)
    : QWidget(parent)
    , m_pipelineManager(pipelineManager)
{
    setupUI();
}

ImageFilterTabWidget::~ImageFilterTabWidget()
{
}

void ImageFilterTabWidget::setupUI()
{
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(10, 10, 10, 10);
    mainLayout->setSpacing(8);

    // 滤波类型选择
    auto* typeLayout = new QHBoxLayout();
    typeLayout->setContentsMargins(0, 0, 0, 0);
    typeLayout->setSpacing(8);
    auto* typeLabel = new QLabel("滤波类型:");
    typeLabel->setFixedWidth(60);
    m_filterTypeCombo = new QComboBox();
    m_filterTypeCombo->addItem("高斯滤波");
    m_filterTypeCombo->addItem("中值滤波");
    m_filterTypeCombo->addItem("双边滤波");
    m_filterTypeCombo->addItem("形态学滤波");
    m_filterTypeCombo->setFixedWidth(150);
    typeLayout->addWidget(typeLabel);
    typeLayout->addWidget(m_filterTypeCombo);
    typeLayout->addStretch();
    mainLayout->addLayout(typeLayout);

    // 参数堆叠容器
    auto* paramStack = new QStackedWidget();

    // === 高斯滤波参数 ===
    m_gaussianParams = new QWidget();
    auto* gaussianLayout = new QVBoxLayout(m_gaussianParams);
    gaussianLayout->setContentsMargins(0, 5, 0, 0);
    gaussianLayout->setSpacing(6);

    // 核大小
    auto* kernelRow = new QHBoxLayout();
    kernelRow->setSpacing(8);
    auto* kernelLabel = new QLabel("核大小:");
    kernelLabel->setFixedWidth(60);
    m_kernelSizeSlider = new QSlider(Qt::Horizontal);
    m_kernelSizeSlider->setRange(1, 15);
    m_kernelSizeSlider->setSingleStep(2);
    m_kernelSizeSlider->setValue(3);
    m_kernelSizeSlider->setFixedWidth(160);
    m_kernelSizeLabel = new QLabel("3");
    m_kernelSizeLabel->setFixedWidth(30);
    kernelRow->addWidget(kernelLabel);
    kernelRow->addWidget(m_kernelSizeSlider);
    kernelRow->addWidget(m_kernelSizeLabel);
    kernelRow->addStretch();
    gaussianLayout->addLayout(kernelRow);

    // σX
    auto* sigmaxRow = new QHBoxLayout();
    sigmaxRow->setSpacing(8);
    auto* sigmaxLabel = new QLabel("σX:");
    sigmaxLabel->setFixedWidth(60);
    m_sigmaXSlider = new QSlider(Qt::Horizontal);
    m_sigmaXSlider->setRange(1, 50);
    m_sigmaXSlider->setValue(10);
    m_sigmaXSlider->setFixedWidth(160);
    m_sigmaXLabel = new QLabel("1.0");
    m_sigmaXLabel->setFixedWidth(30);
    sigmaxRow->addWidget(sigmaxLabel);
    sigmaxRow->addWidget(m_sigmaXSlider);
    sigmaxRow->addWidget(m_sigmaXLabel);
    sigmaxRow->addStretch();
    gaussianLayout->addLayout(sigmaxRow);

    // σY
    auto* sigmayRow = new QHBoxLayout();
    sigmayRow->setSpacing(8);
    auto* sigmayLabel = new QLabel("σY:");
    sigmayLabel->setFixedWidth(60);
    m_sigmaYSlider = new QSlider(Qt::Horizontal);
    m_sigmaYSlider->setRange(0, 50);
    m_sigmaYSlider->setValue(0);
    m_sigmaYSlider->setFixedWidth(160);
    m_sigmaYLabel = new QLabel("0.0");
    m_sigmaYLabel->setFixedWidth(30);
    sigmayRow->addWidget(sigmayLabel);
    sigmayRow->addWidget(m_sigmaYSlider);
    sigmayRow->addWidget(m_sigmaYLabel);
    sigmayRow->addStretch();
    gaussianLayout->addLayout(sigmayRow);

    paramStack->addWidget(m_gaussianParams);

    // === 中值滤波参数 ===
    m_medianParams = new QWidget();
    auto* medianLayout = new QVBoxLayout(m_medianParams);
    medianLayout->setContentsMargins(0, 5, 0, 0);
    medianLayout->setSpacing(6);

    auto* medianKernelRow = new QHBoxLayout();
    medianKernelRow->setSpacing(8);
    auto* medianKernelLabel = new QLabel("核大小:");
    medianKernelLabel->setFixedWidth(60);
    m_medianKernelSlider = new QSlider(Qt::Horizontal);
    m_medianKernelSlider->setRange(1, 15);
    m_medianKernelSlider->setSingleStep(2);
    m_medianKernelSlider->setValue(3);
    m_medianKernelSlider->setFixedWidth(160);
    m_medianKernelLabel = new QLabel("3");
    m_medianKernelLabel->setFixedWidth(30);
    medianKernelRow->addWidget(medianKernelLabel);
    medianKernelRow->addWidget(m_medianKernelSlider);
    medianKernelRow->addWidget(m_medianKernelLabel);
    medianKernelRow->addStretch();
    medianLayout->addLayout(medianKernelRow);

    connect(m_medianKernelSlider, &QSlider::valueChanged, this, [this](int v) {
        m_medianKernelLabel->setText(QString::number(v * 2 + 1));
    });
    paramStack->addWidget(m_medianParams);

    // === 双边滤波参数 ===
    m_bilateralParams = new QWidget();
    auto* bilateralLayout = new QVBoxLayout(m_bilateralParams);
    bilateralLayout->setContentsMargins(0, 5, 0, 0);
    bilateralLayout->setSpacing(6);

    // 直径d
    auto* dRow = new QHBoxLayout();
    dRow->setSpacing(8);
    auto* dLabel = new QLabel("直径d:");
    dLabel->setFixedWidth(60);
    m_bilateralDSlider = new QSlider(Qt::Horizontal);
    m_bilateralDSlider->setRange(1, 30);
    m_bilateralDSlider->setValue(9);
    m_bilateralDSlider->setFixedWidth(160);
    m_bilateralDLabel = new QLabel("9");
    m_bilateralDLabel->setFixedWidth(30);
    dRow->addWidget(dLabel);
    dRow->addWidget(m_bilateralDSlider);
    dRow->addWidget(m_bilateralDLabel);
    dRow->addStretch();
    bilateralLayout->addLayout(dRow);

    // σColor
    auto* sigmaCRow = new QHBoxLayout();
    sigmaCRow->setSpacing(8);
    auto* sigmaCLabel = new QLabel("σColor:");
    sigmaCLabel->setFixedWidth(60);
    m_bilateralSigmaCSlider = new QSlider(Qt::Horizontal);
    m_bilateralSigmaCSlider->setRange(1, 200);
    m_bilateralSigmaCSlider->setValue(75);
    m_bilateralSigmaCSlider->setFixedWidth(160);
    m_bilateralSigmaCLabel = new QLabel("75.0");
    m_bilateralSigmaCLabel->setFixedWidth(30);
    sigmaCRow->addWidget(sigmaCLabel);
    sigmaCRow->addWidget(m_bilateralSigmaCSlider);
    sigmaCRow->addWidget(m_bilateralSigmaCLabel);
    sigmaCRow->addStretch();
    bilateralLayout->addLayout(sigmaCRow);

    // σSpace
    auto* sigmaSRow = new QHBoxLayout();
    sigmaSRow->setSpacing(8);
    auto* sigmaSLabel = new QLabel("σSpace:");
    sigmaSLabel->setFixedWidth(60);
    m_bilateralSigmaSSlider = new QSlider(Qt::Horizontal);
    m_bilateralSigmaSSlider->setRange(1, 200);
    m_bilateralSigmaSSlider->setValue(75);
    m_bilateralSigmaSSlider->setFixedWidth(160);
    m_bilateralSigmaSLabel = new QLabel("75.0");
    m_bilateralSigmaSLabel->setFixedWidth(30);
    sigmaSRow->addWidget(sigmaSLabel);
    sigmaSRow->addWidget(m_bilateralSigmaSSlider);
    sigmaSRow->addWidget(m_bilateralSigmaSLabel);
    sigmaSRow->addStretch();
    bilateralLayout->addLayout(sigmaSRow);

    paramStack->addWidget(m_bilateralParams);

    // === 形态学滤波参数 ===
    m_morphologyParams = new QWidget();
    auto* morphologyLayout = new QVBoxLayout(m_morphologyParams);
    morphologyLayout->setContentsMargins(0, 5, 0, 0);
    morphologyLayout->setSpacing(6);

    // 操作类型
    auto* opRow = new QHBoxLayout();
    opRow->setSpacing(8);
    auto* opLabel = new QLabel("操作:");
    opLabel->setFixedWidth(60);
    m_morphOpCombo = new QComboBox();
    m_morphOpCombo->addItem("开运算 (去小颗粒)");
    m_morphOpCombo->addItem("闭运算 (填小孔)");
    m_morphOpCombo->addItem("腐蚀");
    m_morphOpCombo->addItem("膨胀");
    m_morphOpCombo->setFixedWidth(150);
    opRow->addWidget(opLabel);
    opRow->addWidget(m_morphOpCombo);
    opRow->addStretch();
    morphologyLayout->addLayout(opRow);

    // 核大小
    auto* morphKernelRow = new QHBoxLayout();
    morphKernelRow->setSpacing(8);
    auto* morphKernelLabel = new QLabel("核大小:");
    morphKernelLabel->setFixedWidth(60);
    m_morphKernelSlider = new QSlider(Qt::Horizontal);
    m_morphKernelSlider->setRange(1, 10);
    m_morphKernelSlider->setValue(3);
    m_morphKernelSlider->setFixedWidth(160);
    m_morphKernelLabel = new QLabel("3");
    m_morphKernelLabel->setFixedWidth(30);
    morphKernelRow->addWidget(morphKernelLabel);
    morphKernelRow->addWidget(m_morphKernelSlider);
    morphKernelRow->addWidget(m_morphKernelLabel);
    morphKernelRow->addStretch();
    morphologyLayout->addLayout(morphKernelRow);

    // 迭代次数
    auto* iterRow = new QHBoxLayout();
    iterRow->setSpacing(8);
    auto* iterLabel = new QLabel("迭代次数:");
    iterLabel->setFixedWidth(60);
    m_morphIterSlider = new QSlider(Qt::Horizontal);
    m_morphIterSlider->setRange(1, 5);
    m_morphIterSlider->setValue(1);
    m_morphIterSlider->setFixedWidth(160);
    m_morphIterLabel = new QLabel("1");
    m_morphIterLabel->setFixedWidth(30);
    iterRow->addWidget(iterLabel);
    iterRow->addWidget(m_morphIterSlider);
    iterRow->addWidget(m_morphIterLabel);
    iterRow->addStretch();
    morphologyLayout->addLayout(iterRow);

    paramStack->addWidget(m_morphologyParams);

    mainLayout->addWidget(paramStack);

    // 连接信号 - slider变化只更新标签，不触发pipeline
    connect(m_filterTypeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &ImageFilterTabWidget::onFilterTypeChanged);
    connect(m_kernelSizeSlider, &QSlider::valueChanged, this, [this](int v) {
        m_kernelSizeLabel->setText(QString::number(v * 2 + 1));
    });
    connect(m_sigmaXSlider, &QSlider::valueChanged, this, [this](int v) {
        m_sigmaXLabel->setText(QString::number(v / 10.0, 'f', 1));
    });
    connect(m_sigmaYSlider, &QSlider::valueChanged, this, [this](int v) {
        m_sigmaYLabel->setText(QString::number(v / 10.0, 'f', 1));
    });
    connect(m_bilateralDSlider, &QSlider::valueChanged, this, [this](int v) {
        m_bilateralDLabel->setText(QString::number(v));
    });
    connect(m_bilateralSigmaCSlider, &QSlider::valueChanged, this, [this](int v) {
        m_bilateralSigmaCLabel->setText(QString::number(v * 1.0, 'f', 1));
    });
    connect(m_bilateralSigmaSSlider, &QSlider::valueChanged, this, [this](int v) {
        m_bilateralSigmaSLabel->setText(QString::number(v * 1.0, 'f', 1));
    });
    connect(m_morphOpCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &ImageFilterTabWidget::onMorphOpChanged);
    connect(m_morphKernelSlider, &QSlider::valueChanged, this, [this](int v) {
        m_morphKernelLabel->setText(QString::number(v));
    });
    connect(m_morphIterSlider, &QSlider::valueChanged, this, [this](int v) {
        m_morphIterLabel->setText(QString::number(v));
    });

    // 按钮区：应用 + 重置
    auto* btnLayout = new QHBoxLayout();
    btnLayout->setSpacing(10);
    auto* applyBtn = new QPushButton("应用");
    applyBtn->setFixedWidth(70);
    auto* resetBtn = new QPushButton("重置参数");
    resetBtn->setFixedWidth(80);
    connect(applyBtn, &QPushButton::clicked, this, [this]() {
        syncConfigToPipeline();
        emit processRequested();
    });
    connect(resetBtn, &QPushButton::clicked, this, &ImageFilterTabWidget::onResetClicked);
    btnLayout->addStretch();
    btnLayout->addWidget(applyBtn);
    btnLayout->addWidget(resetBtn);
    mainLayout->addLayout(btnLayout);

    mainLayout->addStretch();
    updateParamVisibility();
}

void ImageFilterTabWidget::onFilterTypeChanged(int index)
{
    Q_UNUSED(index);
    updateParamVisibility();
}

void ImageFilterTabWidget::updateParamVisibility()
{
    int type = m_filterTypeCombo->currentIndex();
    m_gaussianParams->setVisible(type == 0);
    m_medianParams->setVisible(type == 1);
    m_bilateralParams->setVisible(type == 2);
    m_morphologyParams->setVisible(type == 3);
}

void ImageFilterTabWidget::syncConfigToPipeline()
{
    if (!m_pipelineManager) return;

    m_pipelineManager->updateConfig([&](PipelineConfig& cfg) {
        cfg.imageFilter.filterType = static_cast<FilterDenoiseType>(m_filterTypeCombo->currentIndex());

        // 高斯参数
        cfg.imageFilter.gaussianKernelSize = m_kernelSizeSlider->value() * 2 + 1;
        cfg.imageFilter.gaussianSigmaX = m_sigmaXSlider->value() / 10.0;
        cfg.imageFilter.gaussianSigmaY = m_sigmaYSlider->value() / 10.0;

        // 中值参数
        cfg.imageFilter.medianKernelSize = m_medianKernelSlider->value() * 2 + 1;

        // 双边参数
        cfg.imageFilter.bilateralD = m_bilateralDSlider->value();
        cfg.imageFilter.bilateralSigmaColor = m_bilateralSigmaCSlider->value();
        cfg.imageFilter.bilateralSigmaSpace = m_bilateralSigmaSSlider->value();

        // 形态学参数
        cfg.imageFilter.morphologyOp = static_cast<MorphologyOpType>(m_morphOpCombo->currentIndex());
        cfg.imageFilter.morphologyKernelSize = m_morphKernelSlider->value();
        cfg.imageFilter.morphologyIterations = m_morphIterSlider->value();
    });

    emit filterConfigChanged();
}

void ImageFilterTabWidget::onMorphOpChanged(int index)
{
    Q_UNUSED(index);
}

void ImageFilterTabWidget::onResetClicked()
{
    if (!m_pipelineManager) return;

    m_filterTypeCombo->setCurrentIndex(0);
    m_kernelSizeSlider->setValue(1);
    m_sigmaXSlider->setValue(10);
    m_sigmaYSlider->setValue(0);
    m_medianKernelSlider->setValue(1);
    m_bilateralDSlider->setValue(9);
    m_bilateralSigmaCSlider->setValue(75);
    m_bilateralSigmaSSlider->setValue(75);
    m_morphOpCombo->setCurrentIndex(0);
    m_morphKernelSlider->setValue(3);
    m_morphIterSlider->setValue(1);

    syncConfigToPipeline();
    emit processRequested();
}

void ImageFilterTabWidget::saveToConfig(PipelineConfig& config) const
{
    config.imageFilter.filterType = static_cast<FilterDenoiseType>(m_filterTypeCombo->currentIndex());
    config.imageFilter.gaussianKernelSize = m_kernelSizeSlider->value() * 2 + 1;
    config.imageFilter.gaussianSigmaX = m_sigmaXSlider->value() / 10.0;
    config.imageFilter.gaussianSigmaY = m_sigmaYSlider->value() / 10.0;
    config.imageFilter.medianKernelSize = m_medianKernelSlider->value() * 2 + 1;
    config.imageFilter.bilateralD = m_bilateralDSlider->value();
    config.imageFilter.bilateralSigmaColor = m_bilateralSigmaCSlider->value();
    config.imageFilter.bilateralSigmaSpace = m_bilateralSigmaSSlider->value();
    config.imageFilter.morphologyOp = static_cast<MorphologyOpType>(m_morphOpCombo->currentIndex());
    config.imageFilter.morphologyKernelSize = m_morphKernelSlider->value();
    config.imageFilter.morphologyIterations = m_morphIterSlider->value();
}

void ImageFilterTabWidget::loadFromConfig(const PipelineConfig& config)
{
    const auto& cfg = config.imageFilter;

    m_filterTypeCombo->setCurrentIndex(static_cast<int>(cfg.filterType));
    m_kernelSizeSlider->setValue((cfg.gaussianKernelSize - 1) / 2);
    m_sigmaXSlider->setValue(static_cast<int>(cfg.gaussianSigmaX * 10));
    m_sigmaYSlider->setValue(static_cast<int>(cfg.gaussianSigmaY * 10));
    m_medianKernelSlider->setValue((cfg.medianKernelSize - 1) / 2);
    m_bilateralDSlider->setValue(cfg.bilateralD);
    m_bilateralSigmaCSlider->setValue(static_cast<int>(cfg.bilateralSigmaColor));
    m_bilateralSigmaSSlider->setValue(static_cast<int>(cfg.bilateralSigmaSpace));
    m_morphOpCombo->setCurrentIndex(static_cast<int>(cfg.morphologyOp));
    m_morphKernelSlider->setValue(cfg.morphologyKernelSize);
    m_morphIterSlider->setValue(cfg.morphologyIterations);
}

void ImageFilterTabWidget::connectSignals(const SignalContext& ctx,
                                          std::function<void()> onExecutePipeline,
                                          std::function<void()> onConfigSaved)
{
    Q_UNUSED(onConfigSaved);

    connect(this, &ImageFilterTabWidget::processRequested, this, [onExecutePipeline]() {
        if (onExecutePipeline) onExecutePipeline();
    });

    connect(this, &ImageFilterTabWidget::filterConfigChanged, this, [onExecutePipeline]() {
        if (onExecutePipeline) onExecutePipeline();
    });
}

void ImageFilterTabWidget::applyConfig()
{
    if (!m_pipelineManager) return;
    loadFromConfig(m_pipelineManager->config());
    updateParamVisibility();
}
