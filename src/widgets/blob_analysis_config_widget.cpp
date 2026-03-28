#include "widgets/blob_analysis_config_widget.h"
#include "logger.h"

BlobAnalysisConfigWidget::BlobAnalysisConfigWidget(QWidget* parent)
    : QWidget(parent)
    , m_tabWidget(nullptr)
    , m_enhancementTab(nullptr)
    , m_filterTab(nullptr)
    , m_correctionTab(nullptr)
    , m_processingTab(nullptr)
    , m_extractionTab(nullptr)
    , m_judgmentTab(nullptr)
    , m_resetButton(nullptr)
{
    initUI();
    initConnections();
    Logger::instance()->info("Blob分析配置组件初始化完成");
}

BlobAnalysisConfigWidget::~BlobAnalysisConfigWidget()
{
}

void BlobAnalysisConfigWidget::setConfig(const BlobAnalysisConfig& config)
{
    updateUIFromConfig(config);
}

BlobAnalysisConfig BlobAnalysisConfigWidget::getConfig() const
{
    return getConfigFromUI();
}

void BlobAnalysisConfigWidget::resetToDefault()
{
    BlobAnalysisConfig defaultConfig;
    updateUIFromConfig(defaultConfig);
    emit configChanged();
}

void BlobAnalysisConfigWidget::initUI()
{
    // 创建主布局
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(5, 5, 5, 5);
    mainLayout->setSpacing(5);

    // 创建标签页控件
    m_tabWidget = new QTabWidget(this);

    // 初始化各个标签页
    m_enhancementTab = new QWidget();
    m_filterTab = new QWidget();
    m_correctionTab = new QWidget();
    m_processingTab = new QWidget();
    m_extractionTab = new QWidget();
    m_judgmentTab = new QWidget();

    // 添加标签页
    m_tabWidget->addTab(m_enhancementTab, "图像增强");
    m_tabWidget->addTab(m_filterTab, "过滤");
    m_tabWidget->addTab(m_correctionTab, "补正");
    m_tabWidget->addTab(m_processingTab, "处理");
    m_tabWidget->addTab(m_extractionTab, "提取");
    m_tabWidget->addTab(m_judgmentTab, "判定");

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
    initFilterTab();
    initCorrectionTab();
    initProcessingTab();
    initExtractionTab();
    initJudgmentTab();
}

void BlobAnalysisConfigWidget::initEnhancementTab()
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

void BlobAnalysisConfigWidget::initFilterTab()
{
    QVBoxLayout* layout = new QVBoxLayout(m_filterTab);
    layout->setContentsMargins(10, 10, 10, 10);
    layout->setSpacing(15);

    // 面积过滤
    QGroupBox* areaGroup = new QGroupBox("面积过滤");
    QGridLayout* areaLayout = new QGridLayout(areaGroup);
    areaLayout->addWidget(new QLabel("最小面积:"), 0, 0);
    m_minAreaSpinBox = new QSpinBox();
    m_minAreaSpinBox->setRange(0, 1000000);
    m_minAreaSpinBox->setValue(100);
    m_minAreaSpinBox->setSuffix(" 像素");
    areaLayout->addWidget(m_minAreaSpinBox, 0, 1);
    
    areaLayout->addWidget(new QLabel("最大面积:"), 1, 0);
    m_maxAreaSpinBox = new QSpinBox();
    m_maxAreaSpinBox->setRange(0, 1000000);
    m_maxAreaSpinBox->setValue(10000);
    m_maxAreaSpinBox->setSuffix(" 像素");
    areaLayout->addWidget(m_maxAreaSpinBox, 1, 1);

    // 圆形度过滤
    QGroupBox* circularityGroup = new QGroupBox("圆形度过滤");
    QGridLayout* circularityLayout = new QGridLayout(circularityGroup);
    circularityLayout->addWidget(new QLabel("最小圆形度:"), 0, 0);
    m_minCircularitySpinBox = new QDoubleSpinBox();
    m_minCircularitySpinBox->setRange(0.0, 1.0);
    m_minCircularitySpinBox->setValue(0.5);
    m_minCircularitySpinBox->setDecimals(2);
    circularityLayout->addWidget(m_minCircularitySpinBox, 0, 1);
    
    circularityLayout->addWidget(new QLabel("最大圆形度:"), 1, 0);
    m_maxCircularitySpinBox = new QDoubleSpinBox();
    m_maxCircularitySpinBox->setRange(0.0, 1.0);
    m_maxCircularitySpinBox->setValue(1.0);
    m_maxCircularitySpinBox->setDecimals(2);
    circularityLayout->addWidget(m_maxCircularitySpinBox, 1, 1);

    // 凸性过滤
    QGroupBox* convexityGroup = new QGroupBox("凸性过滤");
    QGridLayout* convexityLayout = new QGridLayout(convexityGroup);
    convexityLayout->addWidget(new QLabel("最小凸性:"), 0, 0);
    m_minConvexitySpinBox = new QDoubleSpinBox();
    m_minConvexitySpinBox->setRange(0.0, 1.0);
    m_minConvexitySpinBox->setValue(0.8);
    m_minConvexitySpinBox->setDecimals(2);
    convexityLayout->addWidget(m_minConvexitySpinBox, 0, 1);
    
    convexityLayout->addWidget(new QLabel("最大凸性:"), 1, 0);
    m_maxConvexitySpinBox = new QDoubleSpinBox();
    m_maxConvexitySpinBox->setRange(0.0, 1.0);
    m_maxConvexitySpinBox->setValue(1.0);
    m_maxConvexitySpinBox->setDecimals(2);
    convexityLayout->addWidget(m_maxConvexitySpinBox, 1, 1);

    // 惯性比过滤
    QGroupBox* inertiaGroup = new QGroupBox("惯性比过滤");
    QGridLayout* inertiaLayout = new QGridLayout(inertiaGroup);
    inertiaLayout->addWidget(new QLabel("最小惯性比:"), 0, 0);
    m_minInertiaSpinBox = new QDoubleSpinBox();
    m_minInertiaSpinBox->setRange(0.0, 1.0);
    m_minInertiaSpinBox->setValue(0.5);
    m_minInertiaSpinBox->setDecimals(2);
    inertiaLayout->addWidget(m_minInertiaSpinBox, 0, 1);
    
    inertiaLayout->addWidget(new QLabel("最大惯性比:"), 1, 0);
    m_maxInertiaSpinBox = new QDoubleSpinBox();
    m_maxInertiaSpinBox->setRange(0.0, 1.0);
    m_maxInertiaSpinBox->setValue(1.0);
    m_maxInertiaSpinBox->setDecimals(2);
    inertiaLayout->addWidget(m_maxInertiaSpinBox, 1, 1);

    layout->addWidget(areaGroup);
    layout->addWidget(circularityGroup);
    layout->addWidget(convexityGroup);
    layout->addWidget(inertiaGroup);
    layout->addStretch();

    m_filterTab->setLayout(layout);
}

void BlobAnalysisConfigWidget::initCorrectionTab()
{
    QVBoxLayout* layout = new QVBoxLayout(m_correctionTab);
    layout->setContentsMargins(10, 10, 10, 10);
    layout->setSpacing(15);

    // 启用补正
    m_enableCorrectionCheckBox = new QCheckBox("启用补正");
    m_enableCorrectionCheckBox->setChecked(false);

    // 补正方法
    QGroupBox* methodGroup = new QGroupBox("补正方法");
    QVBoxLayout* methodLayout = new QVBoxLayout(methodGroup);
    m_correctionMethodComboBox = new QComboBox();
    m_correctionMethodComboBox->addItem("无", 0);
    m_correctionMethodComboBox->addItem("平移补正", 1);
    m_correctionMethodComboBox->addItem("旋转补正", 2);
    m_correctionMethodComboBox->addItem("仿射补正", 3);
    m_correctionMethodComboBox->setCurrentIndex(0);
    methodLayout->addWidget(m_correctionMethodComboBox);

    // 补正阈值
    QGroupBox* thresholdGroup = new QGroupBox("补正阈值");
    QVBoxLayout* thresholdLayout = new QVBoxLayout(thresholdGroup);
    m_correctionThresholdSpinBox = new QDoubleSpinBox();
    m_correctionThresholdSpinBox->setRange(0.0, 1.0);
    m_correctionThresholdSpinBox->setValue(0.8);
    m_correctionThresholdSpinBox->setDecimals(2);
    m_correctionThresholdSpinBox->setSingleStep(0.1);
    thresholdLayout->addWidget(m_correctionThresholdSpinBox);

    layout->addWidget(m_enableCorrectionCheckBox);
    layout->addWidget(methodGroup);
    layout->addWidget(thresholdGroup);
    layout->addStretch();

    m_correctionTab->setLayout(layout);
}

void BlobAnalysisConfigWidget::initProcessingTab()
{
    QVBoxLayout* layout = new QVBoxLayout(m_processingTab);
    layout->setContentsMargins(10, 10, 10, 10);
    layout->setSpacing(15);

    // 阈值类型
    QGroupBox* thresholdTypeGroup = new QGroupBox("阈值类型");
    QVBoxLayout* thresholdTypeLayout = new QVBoxLayout(thresholdTypeGroup);
    m_thresholdTypeComboBox = new QComboBox();
    m_thresholdTypeComboBox->addItem("二进制", 0);
    m_thresholdTypeComboBox->addItem("反二进制", 1);
    m_thresholdTypeComboBox->addItem("截断", 2);
    m_thresholdTypeComboBox->addItem("大津法", 3);
    m_thresholdTypeComboBox->setCurrentIndex(0);
    thresholdTypeLayout->addWidget(m_thresholdTypeComboBox);

    // 阈值
    QGroupBox* thresholdValueGroup = new QGroupBox("阈值");
    QVBoxLayout* thresholdValueLayout = new QVBoxLayout(thresholdValueGroup);
    m_thresholdValueSpinBox = new QSpinBox();
    m_thresholdValueSpinBox->setRange(0, 255);
    m_thresholdValueSpinBox->setValue(128);
    thresholdValueLayout->addWidget(m_thresholdValueSpinBox);

    // 反转二值图像
    m_invertBinaryCheckBox = new QCheckBox("反转二值图像");
    m_invertBinaryCheckBox->setChecked(false);

    // 形态学操作
    QGroupBox* morphologyGroup = new QGroupBox("形态学操作");
    QGridLayout* morphologyLayout = new QGridLayout(morphologyGroup);
    morphologyLayout->addWidget(new QLabel("操作类型:"), 0, 0);
    m_morphologyOpComboBox = new QComboBox();
    m_morphologyOpComboBox->addItem("无", 0);
    m_morphologyOpComboBox->addItem("腐蚀", 1);
    m_morphologyOpComboBox->addItem("膨胀", 2);
    m_morphologyOpComboBox->addItem("开运算", 3);
    m_morphologyOpComboBox->addItem("闭运算", 4);
    m_morphologyOpComboBox->setCurrentIndex(0);
    morphologyLayout->addWidget(m_morphologyOpComboBox, 0, 1);
    
    morphologyLayout->addWidget(new QLabel("核大小:"), 1, 0);
    m_morphologySizeSpinBox = new QSpinBox();
    m_morphologySizeSpinBox->setRange(1, 21);
    m_morphologySizeSpinBox->setValue(3);
    m_morphologySizeSpinBox->setSingleStep(2);
    m_morphologySizeSpinBox->setSuffix(" 像素");
    morphologyLayout->addWidget(m_morphologySizeSpinBox, 1, 1);

    layout->addWidget(thresholdTypeGroup);
    layout->addWidget(thresholdValueGroup);
    layout->addWidget(m_invertBinaryCheckBox);
    layout->addWidget(morphologyGroup);
    layout->addStretch();

    m_processingTab->setLayout(layout);
}

void BlobAnalysisConfigWidget::initExtractionTab()
{
    QVBoxLayout* layout = new QVBoxLayout(m_extractionTab);
    layout->setContentsMargins(10, 10, 10, 10);
    layout->setSpacing(15);

    // 提取方法
    QGroupBox* methodGroup = new QGroupBox("提取方法");
    QVBoxLayout* methodLayout = new QVBoxLayout(methodGroup);
    m_extractionMethodComboBox = new QComboBox();
    m_extractionMethodComboBox->addItem("轮廓提取", 0);
    m_extractionMethodComboBox->addItem("连通域提取", 1);
    m_extractionMethodComboBox->addItem("特征点提取", 2);
    m_extractionMethodComboBox->setCurrentIndex(0);
    methodLayout->addWidget(m_extractionMethodComboBox);

    // 使用轮廓层次结构
    m_useHierarchyCheckBox = new QCheckBox("使用轮廓层次结构");
    m_useHierarchyCheckBox->setChecked(true);

    // 轮廓模式
    QGroupBox* contourModeGroup = new QGroupBox("轮廓模式");
    QVBoxLayout* contourModeLayout = new QVBoxLayout(contourModeGroup);
    m_contourModeComboBox = new QComboBox();
    m_contourModeComboBox->addItem("外轮廓", 0);
    m_contourModeComboBox->addItem("所有轮廓", 1);
    m_contourModeComboBox->addItem("简单轮廓", 2);
    m_contourModeComboBox->setCurrentIndex(0);
    contourModeLayout->addWidget(m_contourModeComboBox);

    layout->addWidget(methodGroup);
    layout->addWidget(m_useHierarchyCheckBox);
    layout->addWidget(contourModeGroup);
    layout->addStretch();

    m_extractionTab->setLayout(layout);
}

void BlobAnalysisConfigWidget::initJudgmentTab()
{
    QVBoxLayout* layout = new QVBoxLayout(m_judgmentTab);
    layout->setContentsMargins(10, 10, 10, 10);
    layout->setSpacing(15);

    // Blob数量
    QGroupBox* countGroup = new QGroupBox("Blob数量");
    QGridLayout* countLayout = new QGridLayout(countGroup);
    countLayout->addWidget(new QLabel("最小数量:"), 0, 0);
    m_minBlobCountSpinBox = new QSpinBox();
    m_minBlobCountSpinBox->setRange(0, 10000);
    m_minBlobCountSpinBox->setValue(0);
    countLayout->addWidget(m_minBlobCountSpinBox, 0, 1);
    
    countLayout->addWidget(new QLabel("最大数量:"), 1, 0);
    m_maxBlobCountSpinBox = new QSpinBox();
    m_maxBlobCountSpinBox->setRange(0, 10000);
    m_maxBlobCountSpinBox->setValue(100);
    countLayout->addWidget(m_maxBlobCountSpinBox, 1, 1);

    // 面积阈值
    QGroupBox* areaThresholdGroup = new QGroupBox("面积阈值");
    QGridLayout* areaThresholdLayout = new QGridLayout(areaThresholdGroup);
    areaThresholdLayout->addWidget(new QLabel("最小面积阈值:"), 0, 0);
    m_minAreaThresholdSpinBox = new QDoubleSpinBox();
    m_minAreaThresholdSpinBox->setRange(0, 1000000);
    m_minAreaThresholdSpinBox->setValue(0);
    m_minAreaThresholdSpinBox->setSuffix(" 像素");
    areaThresholdLayout->addWidget(m_minAreaThresholdSpinBox, 0, 1);
    
    areaThresholdLayout->addWidget(new QLabel("最大面积阈值:"), 1, 0);
    m_maxAreaThresholdSpinBox = new QDoubleSpinBox();
    m_maxAreaThresholdSpinBox->setRange(0, 1000000);
    m_maxAreaThresholdSpinBox->setValue(1000000);
    m_maxAreaThresholdSpinBox->setSuffix(" 像素");
    areaThresholdLayout->addWidget(m_maxAreaThresholdSpinBox, 1, 1);

    layout->addWidget(countGroup);
    layout->addWidget(areaThresholdGroup);
    layout->addStretch();

    m_judgmentTab->setLayout(layout);
}

void BlobAnalysisConfigWidget::initConnections()
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
    connect(m_brightnessSlider, &QSlider::valueChanged, this, &BlobAnalysisConfigWidget::onEnhancementChanged);
    connect(m_contrastSlider, &QSlider::valueChanged, this, &BlobAnalysisConfigWidget::onEnhancementChanged);
    connect(m_gammaSlider, &QSlider::valueChanged, this, &BlobAnalysisConfigWidget::onEnhancementChanged);
    connect(m_sharpenSlider, &QSlider::valueChanged, this, &BlobAnalysisConfigWidget::onEnhancementChanged);

    connect(m_minAreaSpinBox, QOverload<int>::of(&QSpinBox::valueChanged), this, &BlobAnalysisConfigWidget::onFilterChanged);
    connect(m_maxAreaSpinBox, QOverload<int>::of(&QSpinBox::valueChanged), this, &BlobAnalysisConfigWidget::onFilterChanged);
    connect(m_minCircularitySpinBox, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &BlobAnalysisConfigWidget::onFilterChanged);
    connect(m_maxCircularitySpinBox, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &BlobAnalysisConfigWidget::onFilterChanged);
    connect(m_minConvexitySpinBox, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &BlobAnalysisConfigWidget::onFilterChanged);
    connect(m_maxConvexitySpinBox, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &BlobAnalysisConfigWidget::onFilterChanged);
    connect(m_minInertiaSpinBox, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &BlobAnalysisConfigWidget::onFilterChanged);
    connect(m_maxInertiaSpinBox, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &BlobAnalysisConfigWidget::onFilterChanged);

    connect(m_enableCorrectionCheckBox, &QCheckBox::stateChanged, this, &BlobAnalysisConfigWidget::onCorrectionChanged);
    connect(m_correctionMethodComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &BlobAnalysisConfigWidget::onCorrectionChanged);
    connect(m_correctionThresholdSpinBox, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &BlobAnalysisConfigWidget::onCorrectionChanged);

    connect(m_thresholdTypeComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &BlobAnalysisConfigWidget::onProcessingChanged);
    connect(m_thresholdValueSpinBox, QOverload<int>::of(&QSpinBox::valueChanged), this, &BlobAnalysisConfigWidget::onProcessingChanged);
    connect(m_invertBinaryCheckBox, &QCheckBox::stateChanged, this, &BlobAnalysisConfigWidget::onProcessingChanged);
    connect(m_morphologyOpComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &BlobAnalysisConfigWidget::onProcessingChanged);
    connect(m_morphologySizeSpinBox, QOverload<int>::of(&QSpinBox::valueChanged), this, &BlobAnalysisConfigWidget::onProcessingChanged);

    connect(m_extractionMethodComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &BlobAnalysisConfigWidget::onExtractionChanged);
    connect(m_useHierarchyCheckBox, &QCheckBox::stateChanged, this, &BlobAnalysisConfigWidget::onExtractionChanged);
    connect(m_contourModeComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &BlobAnalysisConfigWidget::onExtractionChanged);

    connect(m_minBlobCountSpinBox, QOverload<int>::of(&QSpinBox::valueChanged), this, &BlobAnalysisConfigWidget::onJudgmentChanged);
    connect(m_maxBlobCountSpinBox, QOverload<int>::of(&QSpinBox::valueChanged), this, &BlobAnalysisConfigWidget::onJudgmentChanged);
    connect(m_minAreaThresholdSpinBox, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &BlobAnalysisConfigWidget::onJudgmentChanged);
    connect(m_maxAreaThresholdSpinBox, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &BlobAnalysisConfigWidget::onJudgmentChanged);

    // 连接重置按钮
    connect(m_resetButton, &QPushButton::clicked, this, &BlobAnalysisConfigWidget::onResetClicked);
}

void BlobAnalysisConfigWidget::onEnhancementChanged()
{
    emit configChanged();
}

void BlobAnalysisConfigWidget::onFilterChanged()
{
    emit configChanged();
}

void BlobAnalysisConfigWidget::onCorrectionChanged()
{
    emit configChanged();
}

void BlobAnalysisConfigWidget::onProcessingChanged()
{
    emit configChanged();
}

void BlobAnalysisConfigWidget::onExtractionChanged()
{
    emit configChanged();
}

void BlobAnalysisConfigWidget::onJudgmentChanged()
{
    emit configChanged();
}

void BlobAnalysisConfigWidget::onResetClicked()
{
    resetToDefault();
}

void BlobAnalysisConfigWidget::updateUIFromConfig(const BlobAnalysisConfig& config)
{
    // 图像增强
    m_brightnessSlider->setValue(config.brightness);
    m_contrastSlider->setValue(config.contrast);
    m_gammaSlider->setValue(config.gamma);
    m_sharpenSlider->setValue(config.sharpen);

    // 过滤
    m_minAreaSpinBox->setValue(config.minArea);
    m_maxAreaSpinBox->setValue(config.maxArea);
    m_minCircularitySpinBox->setValue(config.minCircularity);
    m_maxCircularitySpinBox->setValue(config.maxCircularity);
    m_minConvexitySpinBox->setValue(config.minConvexity);
    m_maxConvexitySpinBox->setValue(config.maxConvexity);
    m_minInertiaSpinBox->setValue(config.minInertia);
    m_maxInertiaSpinBox->setValue(config.maxInertia);

    // 补正
    m_enableCorrectionCheckBox->setChecked(config.enableCorrection);
    m_correctionMethodComboBox->setCurrentIndex(config.correctionMethod);
    m_correctionThresholdSpinBox->setValue(config.correctionThreshold);

    // 处理
    m_thresholdTypeComboBox->setCurrentIndex(config.thresholdType);
    m_thresholdValueSpinBox->setValue(config.thresholdValue);
    m_invertBinaryCheckBox->setChecked(config.invertBinary);
    m_morphologyOpComboBox->setCurrentIndex(config.morphologyOp);
    m_morphologySizeSpinBox->setValue(config.morphologySize);

    // 提取
    m_extractionMethodComboBox->setCurrentIndex(config.extractionMethod);
    m_useHierarchyCheckBox->setChecked(config.useHierarchy);
    m_contourModeComboBox->setCurrentIndex(config.contourMode);

    // 判定
    m_minBlobCountSpinBox->setValue(config.minBlobCount);
    m_maxBlobCountSpinBox->setValue(config.maxBlobCount);
    m_minAreaThresholdSpinBox->setValue(config.minAreaThreshold);
    m_maxAreaThresholdSpinBox->setValue(config.maxAreaThreshold);
}

BlobAnalysisConfig BlobAnalysisConfigWidget::getConfigFromUI() const
{
    BlobAnalysisConfig config;

    // 图像增强
    config.brightness = m_brightnessSlider->value();
    config.contrast = m_contrastSlider->value();
    config.gamma = m_gammaSlider->value();
    config.sharpen = m_sharpenSlider->value();

    // 过滤
    config.minArea = m_minAreaSpinBox->value();
    config.maxArea = m_maxAreaSpinBox->value();
    config.minCircularity = m_minCircularitySpinBox->value();
    config.maxCircularity = m_maxCircularitySpinBox->value();
    config.minConvexity = m_minConvexitySpinBox->value();
    config.maxConvexity = m_maxConvexitySpinBox->value();
    config.minInertia = m_minInertiaSpinBox->value();
    config.maxInertia = m_maxInertiaSpinBox->value();

    // 补正
    config.enableCorrection = m_enableCorrectionCheckBox->isChecked();
    config.correctionMethod = m_correctionMethodComboBox->currentIndex();
    config.correctionThreshold = m_correctionThresholdSpinBox->value();

    // 处理
    config.thresholdType = m_thresholdTypeComboBox->currentIndex();
    config.thresholdValue = m_thresholdValueSpinBox->value();
    config.invertBinary = m_invertBinaryCheckBox->isChecked();
    config.morphologyOp = m_morphologyOpComboBox->currentIndex();
    config.morphologySize = m_morphologySizeSpinBox->value();

    // 提取
    config.extractionMethod = m_extractionMethodComboBox->currentIndex();
    config.useHierarchy = m_useHierarchyCheckBox->isChecked();
    config.contourMode = m_contourModeComboBox->currentIndex();

    // 判定
    config.minBlobCount = m_minBlobCountSpinBox->value();
    config.maxBlobCount = m_maxBlobCountSpinBox->value();
    config.minAreaThreshold = m_minAreaThresholdSpinBox->value();
    config.maxAreaThreshold = m_maxAreaThresholdSpinBox->value();

    return config;
}