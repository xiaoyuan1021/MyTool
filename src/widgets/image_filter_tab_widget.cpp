#include "image_filter_tab_widget.h"
#include "config/pipeline_config.h"
#include "controllers/roi_ui_controller.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QPushButton>
#include <QFormLayout>
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
    mainLayout->setContentsMargins(8, 8, 8, 8);

    // 滤波类型选择
    auto* typeGroup = new QGroupBox("滤波类型");
    auto* typeLayout = new QFormLayout(typeGroup);
    m_filterTypeCombo = new QComboBox();
    m_filterTypeCombo->addItem("高斯滤波");
    m_filterTypeCombo->addItem("中值滤波");
    m_filterTypeCombo->addItem("双边滤波");
    m_filterTypeCombo->addItem("形态学滤波");
    typeLayout->addRow("类型:", m_filterTypeCombo);
    mainLayout->addWidget(typeGroup);

    // 参数堆叠容器
    auto* paramStack = new QStackedWidget();

    // === 高斯滤波参数 ===
    m_gaussianParams = new QWidget();
    auto* gaussianLayout = new QFormLayout(m_gaussianParams);
    m_kernelSizeSlider = new QSlider(Qt::Horizontal);
    m_kernelSizeSlider->setRange(1, 15);
    m_kernelSizeSlider->setSingleStep(2);
    m_kernelSizeSlider->setValue(3);
    m_kernelSizeLabel = new QLabel("3");
    auto* kernelLayout = new QHBoxLayout();
    kernelLayout->addWidget(m_kernelSizeSlider);
    kernelLayout->addWidget(m_kernelSizeLabel);
    gaussianLayout->addRow("核大小:", kernelLayout);

    m_sigmaXSlider = new QSlider(Qt::Horizontal);
    m_sigmaXSlider->setRange(1, 50);
    m_sigmaXSlider->setValue(10);
    m_sigmaXLabel = new QLabel("1.0");
    auto* sigmaxLayout = new QHBoxLayout();
    sigmaxLayout->addWidget(m_sigmaXSlider);
    sigmaxLayout->addWidget(m_sigmaXLabel);
    gaussianLayout->addRow("σX:", sigmaxLayout);

    m_sigmaYSlider = new QSlider(Qt::Horizontal);
    m_sigmaYSlider->setRange(0, 50);
    m_sigmaYSlider->setValue(0);
    m_sigmaYLabel = new QLabel("0.0");
    auto* sigmayLayout = new QHBoxLayout();
    sigmayLayout->addWidget(m_sigmaYSlider);
    sigmayLayout->addWidget(m_sigmaYLabel);
    gaussianLayout->addRow("σY:", sigmayLayout);
    paramStack->addWidget(m_gaussianParams);

    // === 中值滤波参数 ===
    m_medianParams = new QWidget();
    auto* medianLayout = new QFormLayout(m_medianParams);
    m_medianKernelSlider = new QSlider(Qt::Horizontal);
    m_medianKernelSlider->setRange(1, 15);
    m_medianKernelSlider->setSingleStep(2);
    m_medianKernelSlider->setValue(3);
    m_medianKernelLabel = new QLabel("3");
    auto* medianKernelLayout = new QHBoxLayout();
    medianKernelLayout->addWidget(m_medianKernelSlider);
    medianKernelLayout->addWidget(m_medianKernelLabel);
    medianLayout->addRow("核大小:", medianKernelLayout);
    connect(m_medianKernelSlider, &QSlider::valueChanged, this, [this](int v) {
        int ksize = v * 2 + 1;
        m_medianKernelLabel->setText(QString::number(ksize));
        syncConfigToPipeline();
        emit processRequested();
    });
    paramStack->addWidget(m_medianParams);

    // === 双边滤波参数 ===
    m_bilateralParams = new QWidget();
    auto* bilateralLayout = new QFormLayout(m_bilateralParams);
    m_bilateralDSlider = new QSlider(Qt::Horizontal);
    m_bilateralDSlider->setRange(1, 30);
    m_bilateralDSlider->setValue(9);
    m_bilateralDLabel = new QLabel("9");
    auto* dLayout = new QHBoxLayout();
    dLayout->addWidget(m_bilateralDSlider);
    dLayout->addWidget(m_bilateralDLabel);
    bilateralLayout->addRow("直径d:", dLayout);

    m_bilateralSigmaCSlider = new QSlider(Qt::Horizontal);
    m_bilateralSigmaCSlider->setRange(1, 200);
    m_bilateralSigmaCSlider->setValue(75);
    m_bilateralSigmaCLabel = new QLabel("75.0");
    auto* sigmaCLayout = new QHBoxLayout();
    sigmaCLayout->addWidget(m_bilateralSigmaCSlider);
    sigmaCLayout->addWidget(m_bilateralSigmaCLabel);
    bilateralLayout->addRow("σColor:", sigmaCLayout);

    m_bilateralSigmaSSlider = new QSlider(Qt::Horizontal);
    m_bilateralSigmaSSlider->setRange(1, 200);
    m_bilateralSigmaSSlider->setValue(75);
    m_bilateralSigmaSLabel = new QLabel("75.0");
    auto* sigmaSLayout = new QHBoxLayout();
    sigmaSLayout->addWidget(m_bilateralSigmaSSlider);
    sigmaSLayout->addWidget(m_bilateralSigmaSLabel);
    bilateralLayout->addRow("σSpace:", sigmaSLayout);
    paramStack->addWidget(m_bilateralParams);

    // === 形态学滤波参数 ===
    m_morphologyParams = new QWidget();
    auto* morphologyLayout = new QFormLayout(m_morphologyParams);
    m_morphOpCombo = new QComboBox();
    m_morphOpCombo->addItem("开运算 (去小颗粒)");
    m_morphOpCombo->addItem("闭运算 (填小孔)");
    m_morphOpCombo->addItem("腐蚀");
    m_morphOpCombo->addItem("膨胀");
    morphologyLayout->addRow("操作:", m_morphOpCombo);

    m_morphKernelSlider = new QSlider(Qt::Horizontal);
    m_morphKernelSlider->setRange(1, 10);
    m_morphKernelSlider->setValue(3);
    m_morphKernelLabel = new QLabel("3");
    auto* morphKernelLayout = new QHBoxLayout();
    morphKernelLayout->addWidget(m_morphKernelSlider);
    morphKernelLayout->addWidget(m_morphKernelLabel);
    morphologyLayout->addRow("核大小:", morphKernelLayout);

    m_morphIterSlider = new QSlider(Qt::Horizontal);
    m_morphIterSlider->setRange(1, 5);
    m_morphIterSlider->setValue(1);
    m_morphIterLabel = new QLabel("1");
    auto* iterLayout = new QHBoxLayout();
    iterLayout->addWidget(m_morphIterSlider);
    iterLayout->addWidget(m_morphIterLabel);
    morphologyLayout->addRow("迭代次数:", iterLayout);
    paramStack->addWidget(m_morphologyParams);

    mainLayout->addWidget(paramStack);

    // 连接信号
    connect(m_filterTypeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &ImageFilterTabWidget::onFilterTypeChanged);
    connect(m_kernelSizeSlider, &QSlider::valueChanged, this, [this](int v) {
        int ksize = v * 2 + 1;
        m_kernelSizeLabel->setText(QString::number(ksize));
        syncConfigToPipeline();
        emit processRequested();
    });
    connect(m_sigmaXSlider, &QSlider::valueChanged, this, [this](int v) {
        m_sigmaXLabel->setText(QString::number(v / 10.0, 'f', 1));
        syncConfigToPipeline();
        emit processRequested();
    });
    connect(m_sigmaYSlider, &QSlider::valueChanged, this, [this](int v) {
        m_sigmaYLabel->setText(QString::number(v / 10.0, 'f', 1));
        syncConfigToPipeline();
        emit processRequested();
    });
    connect(m_bilateralDSlider, &QSlider::valueChanged, this, [this](int v) {
        m_bilateralDLabel->setText(QString::number(v));
        syncConfigToPipeline();
        emit processRequested();
    });
    connect(m_bilateralSigmaCSlider, &QSlider::valueChanged, this, [this](int v) {
        m_bilateralSigmaCLabel->setText(QString::number(v * 1.0, 'f', 1));
        syncConfigToPipeline();
        emit processRequested();
    });
    connect(m_bilateralSigmaSSlider, &QSlider::valueChanged, this, [this](int v) {
        m_bilateralSigmaSLabel->setText(QString::number(v * 1.0, 'f', 1));
        syncConfigToPipeline();
        emit processRequested();
    });
    connect(m_morphOpCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &ImageFilterTabWidget::onMorphOpChanged);
    connect(m_morphKernelSlider, &QSlider::valueChanged, this, [this](int v) {
        m_morphKernelLabel->setText(QString::number(v));
        syncConfigToPipeline();
        emit processRequested();
    });
    connect(m_morphIterSlider, &QSlider::valueChanged, this, [this](int v) {
        m_morphIterLabel->setText(QString::number(v));
        syncConfigToPipeline();
        emit processRequested();
    });

    // 重置按钮
    auto* btnLayout = new QHBoxLayout();
    auto* resetBtn = new QPushButton("重置参数");
    connect(resetBtn, &QPushButton::clicked, this, &ImageFilterTabWidget::onResetClicked);
    btnLayout->addStretch();
    btnLayout->addWidget(resetBtn);
    mainLayout->addLayout(btnLayout);

    mainLayout->addStretch();
    updateParamVisibility();
}

void ImageFilterTabWidget::onFilterTypeChanged(int index)
{
    Q_UNUSED(index);
    updateParamVisibility();
    syncConfigToPipeline();
    emit processRequested();
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

    auto& cfg = m_pipelineManager->mutableConfig().imageFilter;
    cfg.filterType = static_cast<FilterDenoiseType>(m_filterTypeCombo->currentIndex());

    // 高斯参数
    cfg.gaussianKernelSize = m_kernelSizeSlider->value() * 2 + 1;
    cfg.gaussianSigmaX = m_sigmaXSlider->value() / 10.0;
    cfg.gaussianSigmaY = m_sigmaYSlider->value() / 10.0;

    // 中值参数
    cfg.medianKernelSize = m_medianKernelSlider->value() * 2 + 1;

    // 双边参数
    cfg.bilateralD = m_bilateralDSlider->value();
    cfg.bilateralSigmaColor = m_bilateralSigmaCSlider->value();
    cfg.bilateralSigmaSpace = m_bilateralSigmaSSlider->value();

    // 形态学参数
    cfg.morphologyOp = static_cast<MorphologyOpType>(m_morphOpCombo->currentIndex());
    cfg.morphologyKernelSize = m_morphKernelSlider->value();
    cfg.morphologyIterations = m_morphIterSlider->value();

    emit filterConfigChanged();
}

void ImageFilterTabWidget::onMorphOpChanged(int index)
{
    Q_UNUSED(index);
    syncConfigToPipeline();
    emit processRequested();
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

    updateParamVisibility();
}

void ImageFilterTabWidget::connectSignals(PipelineManager* pm, RoiManager* rm,
                                          ImageView* view, RoiUiController* roiCtrl,
                                          std::function<void()> requestRefresh,
                                          std::function<void()> processAndDisplay)
{
    Q_UNUSED(rm);
    Q_UNUSED(view);
    Q_UNUSED(roiCtrl);

    connect(this, &ImageFilterTabWidget::processRequested, this, [processAndDisplay]() {
        if (processAndDisplay) processAndDisplay();
    });

    connect(this, &ImageFilterTabWidget::filterConfigChanged, this, [requestRefresh]() {
        if (requestRefresh) requestRefresh();
    });
}

void ImageFilterTabWidget::applyConfig()
{
    if (!m_pipelineManager) return;
    loadFromConfig(m_pipelineManager->config());
    updateParamVisibility();
}
