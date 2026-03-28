#include "widgets/line_detection_config_widget.h"
#include "logger.h"

LineDetectionConfigWidget::LineDetectionConfigWidget(QWidget* parent)
    : QWidget(parent)
    , m_tabWidget(nullptr)
    , m_enhancementTab(nullptr)
    , m_lineDetectionTab(nullptr)
    , m_resetButton(nullptr)
{
    initUI();
    initConnections();
    Logger::instance()->info("直线检测配置组件初始化完成");
}

LineDetectionConfigWidget::~LineDetectionConfigWidget()
{
}

void LineDetectionConfigWidget::setConfig(const LineDetectionConfig& config)
{
    updateUIFromConfig(config);
}

LineDetectionConfig LineDetectionConfigWidget::getConfig() const
{
    return getConfigFromUI();
}

void LineDetectionConfigWidget::resetToDefault()
{
    LineDetectionConfig defaultConfig;
    updateUIFromConfig(defaultConfig);
    emit configChanged();
}

void LineDetectionConfigWidget::initUI()
{
    // 创建主布局
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(5, 5, 5, 5);
    mainLayout->setSpacing(5);

    // 创建标签页控件
    m_tabWidget = new QTabWidget(this);

    // 初始化各个标签页
    m_enhancementTab = new QWidget();
    m_lineDetectionTab = new QWidget();

    // 添加标签页
    m_tabWidget->addTab(m_enhancementTab, "图像增强");
    m_tabWidget->addTab(m_lineDetectionTab, "直线检测");

    mainLayout->addWidget(m_tabWidget);

    // 创建重置按钮
    m_resetButton = new QPushButton("重置为默认", this);
    m_resetButton->setStyleSheet(
        "QPushButton { "
        "background-color: #ff6b6b; "
        "color: white; "
        "border: none; "
        "padding: 8px 16px; "
        "border-radius: 4px; "
        "font-weight: bold; "
        "} "
        "QPushButton:hover { "
        "background-color: #ff5252; "
        "} "
        "QPushButton:pressed { "
        "background-color: #ff1744; "
        "}"
    );

    mainLayout->addWidget(m_resetButton);

    setLayout(mainLayout);

    // 初始化各个标签页的内容
    initEnhancementTab();
    initLineDetectionTab();
}

void LineDetectionConfigWidget::initEnhancementTab()
{
    QVBoxLayout* layout = new QVBoxLayout(m_enhancementTab);
    layout->setContentsMargins(10, 10, 10, 10);
    layout->setSpacing(15);

    // 亮度控制
    QGroupBox* brightnessGroup = new QGroupBox("亮度");
    QHBoxLayout* brightnessLayout = new QHBoxLayout(brightnessGroup);
    m_brightnessSlider = new QSlider(Qt::Horizontal);
    m_brightnessSlider->setRange(-100, 100);
    m_brightnessSlider->setValue(0);
    m_brightnessSpinBox = new QSpinBox();
    m_brightnessSpinBox->setRange(-100, 100);
    m_brightnessSpinBox->setValue(0);
    m_brightnessSpinBox->setSuffix("");
    m_brightnessSpinBox->setFixedWidth(80);
    brightnessLayout->addWidget(m_brightnessSlider);
    brightnessLayout->addWidget(m_brightnessSpinBox);

    // 对比度控制
    QGroupBox* contrastGroup = new QGroupBox("对比度");
    QHBoxLayout* contrastLayout = new QHBoxLayout(contrastGroup);
    m_contrastSlider = new QSlider(Qt::Horizontal);
    m_contrastSlider->setRange(0, 200);
    m_contrastSlider->setValue(100);
    m_contrastSpinBox = new QSpinBox();
    m_contrastSpinBox->setRange(0, 200);
    m_contrastSpinBox->setValue(100);
    m_contrastSpinBox->setSuffix("%");
    m_contrastSpinBox->setFixedWidth(80);
    contrastLayout->addWidget(m_contrastSlider);
    contrastLayout->addWidget(m_contrastSpinBox);

    // 伽马控制
    QGroupBox* gammaGroup = new QGroupBox("伽马值");
    QHBoxLayout* gammaLayout = new QHBoxLayout(gammaGroup);
    m_gammaSlider = new QSlider(Qt::Horizontal);
    m_gammaSlider->setRange(10, 300);
    m_gammaSlider->setValue(100);
    m_gammaSpinBox = new QSpinBox();
    m_gammaSpinBox->setRange(10, 300);
    m_gammaSpinBox->setValue(100);
    m_gammaSpinBox->setSuffix("%");
    m_gammaSpinBox->setFixedWidth(80);
    gammaLayout->addWidget(m_gammaSlider);
    gammaLayout->addWidget(m_gammaSpinBox);

    // 锐化控制
    QGroupBox* sharpenGroup = new QGroupBox("锐化");
    QHBoxLayout* sharpenLayout = new QHBoxLayout(sharpenGroup);
    m_sharpenSlider = new QSlider(Qt::Horizontal);
    m_sharpenSlider->setRange(0, 200);
    m_sharpenSlider->setValue(100);
    m_sharpenSpinBox = new QSpinBox();
    m_sharpenSpinBox->setRange(0, 200);
    m_sharpenSpinBox->setValue(100);
    m_sharpenSpinBox->setSuffix("%");
    m_sharpenSpinBox->setFixedWidth(80);
    sharpenLayout->addWidget(m_sharpenSlider);
    sharpenLayout->addWidget(m_sharpenSpinBox);

    layout->addWidget(brightnessGroup);
    layout->addWidget(contrastGroup);
    layout->addWidget(gammaGroup);
    layout->addWidget(sharpenGroup);
    layout->addStretch();

    m_enhancementTab->setLayout(layout);
}

void LineDetectionConfigWidget::initLineDetectionTab()
{
    QVBoxLayout* layout = new QVBoxLayout(m_lineDetectionTab);
    layout->setContentsMargins(10, 10, 10, 10);
    layout->setSpacing(15);

    // 算法选择
    QGroupBox* algorithmGroup = new QGroupBox("检测算法");
    QVBoxLayout* algorithmLayout = new QVBoxLayout(algorithmGroup);
    m_algorithmComboBox = new QComboBox();
    m_algorithmComboBox->addItem("HoughP", 0);
    m_algorithmComboBox->addItem("LSD", 1);
    m_algorithmComboBox->addItem("EDline", 2);
    m_algorithmComboBox->setCurrentIndex(0);
    algorithmLayout->addWidget(m_algorithmComboBox);

    // 检测参数
    QGroupBox* paramsGroup = new QGroupBox("检测参数");
    QGridLayout* paramsLayout = new QGridLayout(paramsGroup);
    
    paramsLayout->addWidget(new QLabel("距离分辨率:"), 0, 0);
    m_rhoSpinBox = new QDoubleSpinBox();
    m_rhoSpinBox->setRange(0.1, 10.0);
    m_rhoSpinBox->setValue(1.0);
    m_rhoSpinBox->setDecimals(1);
    m_rhoSpinBox->setSingleStep(0.1);
    paramsLayout->addWidget(m_rhoSpinBox, 0, 1);
    
    paramsLayout->addWidget(new QLabel("角度分辨率:"), 1, 0);
    m_thetaSpinBox = new QDoubleSpinBox();
    m_thetaSpinBox->setRange(0.1, 10.0);
    m_thetaSpinBox->setValue(1.0);
    m_thetaSpinBox->setDecimals(1);
    m_thetaSpinBox->setSingleStep(0.1);
    m_thetaSpinBox->setSuffix(" 度");
    paramsLayout->addWidget(m_thetaSpinBox, 1, 1);
    
    paramsLayout->addWidget(new QLabel("阈值:"), 2, 0);
    m_thresholdSpinBox = new QSpinBox();
    m_thresholdSpinBox->setRange(1, 255);
    m_thresholdSpinBox->setValue(50);
    paramsLayout->addWidget(m_thresholdSpinBox, 2, 1);
    
    paramsLayout->addWidget(new QLabel("最小线段长度:"), 3, 0);
    m_minLineLengthSpinBox = new QDoubleSpinBox();
    m_minLineLengthSpinBox->setRange(0.0, 1000.0);
    m_minLineLengthSpinBox->setValue(30.0);
    m_minLineLengthSpinBox->setDecimals(1);
    m_minLineLengthSpinBox->setSuffix(" 像素");
    paramsLayout->addWidget(m_minLineLengthSpinBox, 3, 1);
    
    paramsLayout->addWidget(new QLabel("最大线段间隙:"), 4, 0);
    m_maxLineGapSpinBox = new QDoubleSpinBox();
    m_maxLineGapSpinBox->setRange(0.0, 100.0);
    m_maxLineGapSpinBox->setValue(10.0);
    m_maxLineGapSpinBox->setDecimals(1);
    m_maxLineGapSpinBox->setSuffix(" 像素");
    paramsLayout->addWidget(m_maxLineGapSpinBox, 4, 1);

    // 参考线匹配
    QGroupBox* referenceGroup = new QGroupBox("参考线匹配");
    QVBoxLayout* referenceLayout = new QVBoxLayout(referenceGroup);
    
    m_enableReferenceLineCheckBox = new QCheckBox("启用参考线匹配");
    m_enableReferenceLineCheckBox->setChecked(false);
    referenceLayout->addWidget(m_enableReferenceLineCheckBox);
    
    QGridLayout* referenceParamsLayout = new QGridLayout();
    referenceParamsLayout->addWidget(new QLabel("角度阈值:"), 0, 0);
    m_angleThresholdSpinBox = new QDoubleSpinBox();
    m_angleThresholdSpinBox->setRange(0.0, 90.0);
    m_angleThresholdSpinBox->setValue(10.0);
    m_angleThresholdSpinBox->setDecimals(1);
    m_angleThresholdSpinBox->setSuffix(" 度");
    referenceParamsLayout->addWidget(m_angleThresholdSpinBox, 0, 1);
    
    referenceParamsLayout->addWidget(new QLabel("距离阈值:"), 1, 0);
    m_distanceThresholdSpinBox = new QDoubleSpinBox();
    m_distanceThresholdSpinBox->setRange(0.0, 100.0);
    m_distanceThresholdSpinBox->setValue(20.0);
    m_distanceThresholdSpinBox->setDecimals(1);
    m_distanceThresholdSpinBox->setSuffix(" 像素");
    referenceParamsLayout->addWidget(m_distanceThresholdSpinBox, 1, 1);
    
    referenceParamsLayout->addWidget(new QLabel("搜索区域宽度:"), 2, 0);
    m_searchRegionWidthSpinBox = new QSpinBox();
    m_searchRegionWidthSpinBox->setRange(1, 200);
    m_searchRegionWidthSpinBox->setValue(50);
    m_searchRegionWidthSpinBox->setSuffix(" 像素");
    referenceParamsLayout->addWidget(m_searchRegionWidthSpinBox, 2, 1);
    
    referenceLayout->addLayout(referenceParamsLayout);

    layout->addWidget(algorithmGroup);
    layout->addWidget(paramsGroup);
    layout->addWidget(referenceGroup);
    layout->addStretch();

    m_lineDetectionTab->setLayout(layout);
}

void LineDetectionConfigWidget::initConnections()
{
    // 连接滑块和数字框
    connect(m_brightnessSlider, &QSlider::valueChanged, m_brightnessSpinBox, &QSpinBox::setValue);
    connect(m_brightnessSpinBox, QOverload<int>::of(&QSpinBox::valueChanged), m_brightnessSlider, &QSlider::setValue);
    connect(m_contrastSlider, &QSlider::valueChanged, m_contrastSpinBox, &QSpinBox::setValue);
    connect(m_contrastSpinBox, QOverload<int>::of(&QSpinBox::valueChanged), m_contrastSlider, &QSlider::setValue);
    connect(m_gammaSlider, &QSlider::valueChanged, m_gammaSpinBox, &QSpinBox::setValue);
    connect(m_gammaSpinBox, QOverload<int>::of(&QSpinBox::valueChanged), m_gammaSlider, &QSlider::setValue);
    connect(m_sharpenSlider, &QSlider::valueChanged, m_sharpenSpinBox, &QSpinBox::setValue);
    connect(m_sharpenSpinBox, QOverload<int>::of(&QSpinBox::valueChanged), m_sharpenSlider, &QSlider::setValue);

    // 连接信号
    connect(m_brightnessSlider, &QSlider::valueChanged, this, &LineDetectionConfigWidget::onEnhancementChanged);
    connect(m_contrastSlider, &QSlider::valueChanged, this, &LineDetectionConfigWidget::onEnhancementChanged);
    connect(m_gammaSlider, &QSlider::valueChanged, this, &LineDetectionConfigWidget::onEnhancementChanged);
    connect(m_sharpenSlider, &QSlider::valueChanged, this, &LineDetectionConfigWidget::onEnhancementChanged);

    connect(m_algorithmComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &LineDetectionConfigWidget::onLineDetectionChanged);
    connect(m_rhoSpinBox, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &LineDetectionConfigWidget::onLineDetectionChanged);
    connect(m_thetaSpinBox, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &LineDetectionConfigWidget::onLineDetectionChanged);
    connect(m_thresholdSpinBox, QOverload<int>::of(&QSpinBox::valueChanged), this, &LineDetectionConfigWidget::onLineDetectionChanged);
    connect(m_minLineLengthSpinBox, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &LineDetectionConfigWidget::onLineDetectionChanged);
    connect(m_maxLineGapSpinBox, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &LineDetectionConfigWidget::onLineDetectionChanged);
    connect(m_enableReferenceLineCheckBox, &QCheckBox::stateChanged, this, &LineDetectionConfigWidget::onLineDetectionChanged);
    connect(m_angleThresholdSpinBox, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &LineDetectionConfigWidget::onLineDetectionChanged);
    connect(m_distanceThresholdSpinBox, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &LineDetectionConfigWidget::onLineDetectionChanged);
    connect(m_searchRegionWidthSpinBox, QOverload<int>::of(&QSpinBox::valueChanged), this, &LineDetectionConfigWidget::onLineDetectionChanged);

    // 连接重置按钮
    connect(m_resetButton, &QPushButton::clicked, this, &LineDetectionConfigWidget::onResetClicked);
}

void LineDetectionConfigWidget::onEnhancementChanged()
{
    emit configChanged();
}

void LineDetectionConfigWidget::onLineDetectionChanged()
{
    emit configChanged();
}

void LineDetectionConfigWidget::onResetClicked()
{
    resetToDefault();
}

void LineDetectionConfigWidget::updateUIFromConfig(const LineDetectionConfig& config)
{
    // 图像增强
    m_brightnessSlider->setValue(config.brightness);
    m_contrastSlider->setValue(config.contrast);
    m_gammaSlider->setValue(config.gamma);
    m_sharpenSlider->setValue(config.sharpen);

    // 直线检测
    m_algorithmComboBox->setCurrentIndex(config.algorithm);
    m_rhoSpinBox->setValue(config.rho);
    m_thetaSpinBox->setValue(config.theta * 180.0 / 3.14159265358979323846); // 转换为度
    m_thresholdSpinBox->setValue(config.threshold);
    m_minLineLengthSpinBox->setValue(config.minLineLength);
    m_maxLineGapSpinBox->setValue(config.maxLineGap);
    m_enableReferenceLineCheckBox->setChecked(config.enableReferenceLine);
    m_angleThresholdSpinBox->setValue(config.angleThreshold);
    m_distanceThresholdSpinBox->setValue(config.distanceThreshold);
    m_searchRegionWidthSpinBox->setValue(config.searchRegionWidth);
}

LineDetectionConfig LineDetectionConfigWidget::getConfigFromUI() const
{
    LineDetectionConfig config;

    // 图像增强
    config.brightness = m_brightnessSlider->value();
    config.contrast = m_contrastSlider->value();
    config.gamma = m_gammaSlider->value();
    config.sharpen = m_sharpenSlider->value();

    // 直线检测
    config.algorithm = m_algorithmComboBox->currentIndex();
    config.rho = m_rhoSpinBox->value();
    config.theta = m_thetaSpinBox->value() * 3.14159265358979323846 / 180.0; // 转换为弧度
    config.threshold = m_thresholdSpinBox->value();
    config.minLineLength = m_minLineLengthSpinBox->value();
    config.maxLineGap = m_maxLineGapSpinBox->value();
    config.enableReferenceLine = m_enableReferenceLineCheckBox->isChecked();
    config.angleThreshold = m_angleThresholdSpinBox->value();
    config.distanceThreshold = m_distanceThresholdSpinBox->value();
    config.searchRegionWidth = m_searchRegionWidthSpinBox->value();

    return config;
}