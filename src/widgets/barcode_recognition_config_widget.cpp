#include "widgets/barcode_recognition_config_widget.h"
#include "logger.h"

BarcodeRecognitionConfigWidget::BarcodeRecognitionConfigWidget(QWidget* parent)
    : QWidget(parent)
    , m_tabWidget(nullptr)
    , m_enhancementTab(nullptr)
    , m_barcodeRecognitionTab(nullptr)
    , m_resetButton(nullptr)
{
    initUI();
    initConnections();
    Logger::instance()->info("条码识别配置组件初始化完成");
}

BarcodeRecognitionConfigWidget::~BarcodeRecognitionConfigWidget()
{
}

void BarcodeRecognitionConfigWidget::setConfig(const BarcodeRecognitionConfig& config)
{
    updateUIFromConfig(config);
}

BarcodeRecognitionConfig BarcodeRecognitionConfigWidget::getConfig() const
{
    return getConfigFromUI();
}

void BarcodeRecognitionConfigWidget::resetToDefault()
{
    BarcodeRecognitionConfig defaultConfig;
    updateUIFromConfig(defaultConfig);
    emit configChanged();
}

void BarcodeRecognitionConfigWidget::initUI()
{
    // 创建主布局
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(5, 5, 5, 5);
    mainLayout->setSpacing(5);

    // 创建标签页控件
    m_tabWidget = new QTabWidget(this);

    // 初始化各个标签页
    m_enhancementTab = new QWidget();
    m_barcodeRecognitionTab = new QWidget();

    // 添加标签页
    m_tabWidget->addTab(m_enhancementTab, "图像增强");
    m_tabWidget->addTab(m_barcodeRecognitionTab, "条码识别");

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
    initBarcodeRecognitionTab();
}

void BarcodeRecognitionConfigWidget::initEnhancementTab()
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

void BarcodeRecognitionConfigWidget::initBarcodeRecognitionTab()
{
    QVBoxLayout* layout = new QVBoxLayout(m_barcodeRecognitionTab);
    layout->setContentsMargins(10, 10, 10, 10);
    layout->setSpacing(15);

    // 启用条码识别
    m_enableBarcodeCheckBox = new QCheckBox("启用条码识别");
    m_enableBarcodeCheckBox->setChecked(true);
    layout->addWidget(m_enableBarcodeCheckBox);

    // 条码类型
    QGroupBox* codeTypeGroup = new QGroupBox("条码类型");
    QVBoxLayout* codeTypeLayout = new QVBoxLayout(codeTypeGroup);
    m_codeTypesComboBox = new QComboBox();
    m_codeTypesComboBox->addItem("自动检测", "auto");
    m_codeTypesComboBox->addItem("QR Code", "QR Code");
    m_codeTypesComboBox->addItem("Data Matrix", "Data Matrix");
    m_codeTypesComboBox->addItem("Code 128", "Code 128");
    m_codeTypesComboBox->addItem("Code 39", "Code 39");
    m_codeTypesComboBox->addItem("EAN-13", "EAN-13");
    m_codeTypesComboBox->addItem("EAN-8", "EAN-8");
    m_codeTypesComboBox->addItem("UPC-A", "UPC-A");
    m_codeTypesComboBox->addItem("UPC-E", "UPC-E");
    m_codeTypesComboBox->setCurrentIndex(0);
    codeTypeLayout->addWidget(m_codeTypesComboBox);

    // 识别参数
    QGroupBox* paramsGroup = new QGroupBox("识别参数");
    QGridLayout* paramsLayout = new QGridLayout(paramsGroup);
    
    paramsLayout->addWidget(new QLabel("最大识别数量:"), 0, 0);
    m_maxNumSymbolsSpinBox = new QSpinBox();
    m_maxNumSymbolsSpinBox->setRange(0, 100);
    m_maxNumSymbolsSpinBox->setValue(0);
    m_maxNumSymbolsSpinBox->setSpecialValueText("不限制");
    m_maxNumSymbolsSpinBox->setSuffix(" 个");
    paramsLayout->addWidget(m_maxNumSymbolsSpinBox, 0, 1);
    
    paramsLayout->addWidget(new QLabel("最小置信度:"), 1, 0);
    m_minConfidenceSpinBox = new QDoubleSpinBox();
    m_minConfidenceSpinBox->setRange(0.0, 1.0);
    m_minConfidenceSpinBox->setValue(0.7);
    m_minConfidenceSpinBox->setDecimals(2);
    m_minConfidenceSpinBox->setSingleStep(0.1);
    paramsLayout->addWidget(m_minConfidenceSpinBox, 1, 1);
    
    paramsLayout->addWidget(new QLabel("超时时间:"), 2, 0);
    m_timeoutSpinBox = new QSpinBox();
    m_timeoutSpinBox->setRange(1000, 30000);
    m_timeoutSpinBox->setValue(5000);
    m_timeoutSpinBox->setSuffix(" 毫秒");
    paramsLayout->addWidget(m_timeoutSpinBox, 2, 1);
    
    m_returnQualityCheckBox = new QCheckBox("返回质量信息");
    m_returnQualityCheckBox->setChecked(true);
    paramsLayout->addWidget(m_returnQualityCheckBox, 3, 0, 1, 2);

    // 预处理参数
    QGroupBox* preprocessGroup = new QGroupBox("预处理参数");
    QVBoxLayout* preprocessLayout = new QVBoxLayout(preprocessGroup);
    
    m_enablePreprocessingCheckBox = new QCheckBox("启用预处理");
    m_enablePreprocessingCheckBox->setChecked(true);
    preprocessLayout->addWidget(m_enablePreprocessingCheckBox);
    
    QGridLayout* preprocessParamsLayout = new QGridLayout();
    preprocessParamsLayout->addWidget(new QLabel("预处理方法:"), 0, 0);
    m_preprocessMethodComboBox = new QComboBox();
    m_preprocessMethodComboBox->addItem("直接识别", 0);
    m_preprocessMethodComboBox->addItem("二值化处理", 1);
    m_preprocessMethodComboBox->addItem("形态学处理", 2);
    m_preprocessMethodComboBox->setCurrentIndex(0);
    preprocessParamsLayout->addWidget(m_preprocessMethodComboBox, 0, 1);
    
    preprocessParamsLayout->addWidget(new QLabel("二值化阈值:"), 1, 0);
    m_binarizationThresholdSpinBox = new QSpinBox();
    m_binarizationThresholdSpinBox->setRange(0, 255);
    m_binarizationThresholdSpinBox->setValue(128);
    m_binarizationThresholdSpinBox->setSuffix(" 像素");
    preprocessParamsLayout->addWidget(m_binarizationThresholdSpinBox, 1, 1);
    
    preprocessParamsLayout->addWidget(new QLabel("形态学核大小:"), 2, 0);
    m_morphologySizeSpinBox = new QSpinBox();
    m_morphologySizeSpinBox->setRange(1, 21);
    m_morphologySizeSpinBox->setValue(3);
    m_morphologySizeSpinBox->setSingleStep(2);
    m_morphologySizeSpinBox->setSuffix(" 像素");
    preprocessParamsLayout->addWidget(m_morphologySizeSpinBox, 2, 1);
    
    preprocessLayout->addLayout(preprocessParamsLayout);

    layout->addWidget(m_enableBarcodeCheckBox);
    layout->addWidget(codeTypeGroup);
    layout->addWidget(paramsGroup);
    layout->addWidget(preprocessGroup);
    layout->addStretch();

    m_barcodeRecognitionTab->setLayout(layout);
}

void BarcodeRecognitionConfigWidget::initConnections()
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
    connect(m_brightnessSlider, &QSlider::valueChanged, this, &BarcodeRecognitionConfigWidget::onEnhancementChanged);
    connect(m_contrastSlider, &QSlider::valueChanged, this, &BarcodeRecognitionConfigWidget::onEnhancementChanged);
    connect(m_gammaSlider, &QSlider::valueChanged, this, &BarcodeRecognitionConfigWidget::onEnhancementChanged);
    connect(m_sharpenSlider, &QSlider::valueChanged, this, &BarcodeRecognitionConfigWidget::onEnhancementChanged);

    connect(m_enableBarcodeCheckBox, &QCheckBox::stateChanged, this, &BarcodeRecognitionConfigWidget::onBarcodeRecognitionChanged);
    connect(m_codeTypesComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &BarcodeRecognitionConfigWidget::onBarcodeRecognitionChanged);
    connect(m_maxNumSymbolsSpinBox, QOverload<int>::of(&QSpinBox::valueChanged), this, &BarcodeRecognitionConfigWidget::onBarcodeRecognitionChanged);
    connect(m_returnQualityCheckBox, &QCheckBox::stateChanged, this, &BarcodeRecognitionConfigWidget::onBarcodeRecognitionChanged);
    connect(m_minConfidenceSpinBox, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &BarcodeRecognitionConfigWidget::onBarcodeRecognitionChanged);
    connect(m_timeoutSpinBox, QOverload<int>::of(&QSpinBox::valueChanged), this, &BarcodeRecognitionConfigWidget::onBarcodeRecognitionChanged);
    connect(m_enablePreprocessingCheckBox, &QCheckBox::stateChanged, this, &BarcodeRecognitionConfigWidget::onBarcodeRecognitionChanged);
    connect(m_preprocessMethodComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &BarcodeRecognitionConfigWidget::onBarcodeRecognitionChanged);
    connect(m_binarizationThresholdSpinBox, QOverload<int>::of(&QSpinBox::valueChanged), this, &BarcodeRecognitionConfigWidget::onBarcodeRecognitionChanged);
    connect(m_morphologySizeSpinBox, QOverload<int>::of(&QSpinBox::valueChanged), this, &BarcodeRecognitionConfigWidget::onBarcodeRecognitionChanged);

    // 连接重置按钮
    connect(m_resetButton, &QPushButton::clicked, this, &BarcodeRecognitionConfigWidget::onResetClicked);
}

void BarcodeRecognitionConfigWidget::onEnhancementChanged()
{
    emit configChanged();
}

void BarcodeRecognitionConfigWidget::onBarcodeRecognitionChanged()
{
    emit configChanged();
}

void BarcodeRecognitionConfigWidget::onResetClicked()
{
    resetToDefault();
}

void BarcodeRecognitionConfigWidget::updateUIFromConfig(const BarcodeRecognitionConfig& config)
{
    // 图像增强
    m_brightnessSlider->setValue(config.brightness);
    m_contrastSlider->setValue(config.contrast);
    m_gammaSlider->setValue(config.gamma);
    m_sharpenSlider->setValue(config.sharpen);

    // 条码识别
    m_enableBarcodeCheckBox->setChecked(config.enableBarcode);
    
    // 设置条码类型
    int codeTypeIndex = m_codeTypesComboBox->findData(config.codeTypes.isEmpty() ? "auto" : config.codeTypes.first());
    if (codeTypeIndex >= 0) {
        m_codeTypesComboBox->setCurrentIndex(codeTypeIndex);
    }
    
    m_maxNumSymbolsSpinBox->setValue(config.maxNumSymbols);
    m_returnQualityCheckBox->setChecked(config.returnQuality);
    m_minConfidenceSpinBox->setValue(config.minConfidence);
    m_timeoutSpinBox->setValue(config.timeout);
    m_enablePreprocessingCheckBox->setChecked(config.enablePreprocessing);
    m_preprocessMethodComboBox->setCurrentIndex(config.preprocessMethod);
    m_binarizationThresholdSpinBox->setValue(config.binarizationThreshold);
    m_morphologySizeSpinBox->setValue(config.morphologySize);
}

BarcodeRecognitionConfig BarcodeRecognitionConfigWidget::getConfigFromUI() const
{
    BarcodeRecognitionConfig config;

    // 图像增强
    config.brightness = m_brightnessSlider->value();
    config.contrast = m_contrastSlider->value();
    config.gamma = m_gammaSlider->value();
    config.sharpen = m_sharpenSlider->value();

    // 条码识别
    config.enableBarcode = m_enableBarcodeCheckBox->isChecked();
    
    // 获取条码类型
    QString codeType = m_codeTypesComboBox->currentData().toString();
    if (codeType == "auto") {
        config.codeTypes = {"auto"};
    } else {
        config.codeTypes = {codeType};
    }
    
    config.maxNumSymbols = m_maxNumSymbolsSpinBox->value();
    config.returnQuality = m_returnQualityCheckBox->isChecked();
    config.minConfidence = m_minConfidenceSpinBox->value();
    config.timeout = m_timeoutSpinBox->value();
    config.enablePreprocessing = m_enablePreprocessingCheckBox->isChecked();
    config.preprocessMethod = m_preprocessMethodComboBox->currentIndex();
    config.binarizationThreshold = m_binarizationThresholdSpinBox->value();
    config.morphologySize = m_morphologySizeSpinBox->value();

    return config;
}