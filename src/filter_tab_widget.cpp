#include "filter_tab_widget.h"
#include "ui_filter_tab.h"
#include "logger.h"
#include <QDebug>

FilterTabWidget::FilterTabWidget(PipelineManager* pipelineManager, QWidget* parent)
    : QWidget(parent)
    , m_ui(new Ui::FilterTabWidget)
    , m_pipelineManager(pipelineManager)
    , m_debounceTimer(new QTimer(this))
{
    m_ui->setupUi(this);

    this->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    this->setMinimumSize(0, 0);
    this->setMaximumSize(QWIDGETSIZE_MAX, QWIDGETSIZE_MAX);
    // 初始化所有 slider 和 spinbox
    // 灰度
    m_ui->Slider_grayLow->setRange(0, 255);
    m_ui->Slider_grayLow->setValue(0);
    m_ui->Slider_grayHigh->setRange(0, 255);
    m_ui->Slider_grayHigh->setValue(120);

    // RGB
    m_ui->Slider_rgb_R_Low->setRange(0, 255);
    m_ui->Slider_rgb_R_Low->setValue(0);
    m_ui->Slider_rgb_R_High->setRange(0, 255);
    m_ui->Slider_rgb_R_High->setValue(120);
    m_ui->Slider_rgb_G_Low->setRange(0, 255);
    m_ui->Slider_rgb_G_Low->setValue(0);
    m_ui->Slider_rgb_G_High->setRange(0, 255);
    m_ui->Slider_rgb_G_High->setValue(120);
    m_ui->Slider_rgb_B_Low->setRange(0, 255);
    m_ui->Slider_rgb_B_Low->setValue(0);
    m_ui->Slider_rgb_B_High->setRange(0, 255);
    m_ui->Slider_rgb_B_High->setValue(120);

    // HSV
    m_ui->Slider_hsv_H_Low->setRange(0, 180);
    m_ui->Slider_hsv_H_Low->setValue(0);
    m_ui->Slider_hsv_H_High->setRange(0, 180);
    m_ui->Slider_hsv_H_High->setValue(120);
    m_ui->Slider_hsv_S_Low->setRange(0, 255);
    m_ui->Slider_hsv_S_Low->setValue(0);
    m_ui->Slider_hsv_S_High->setRange(0, 255);
    m_ui->Slider_hsv_S_High->setValue(120);
    m_ui->Slider_hsv_V_Low->setRange(0, 255);
    m_ui->Slider_hsv_V_Low->setValue(0);
    m_ui->Slider_hsv_V_High->setRange(0, 255);
    m_ui->Slider_hsv_V_High->setValue(120);

    // 初始化 spinbox 范围
    m_ui->spinBox_grayLow->setRange(0, 255);
    m_ui->spinBox_grayLow->setValue(0);
    m_ui->spinBox_grayHigh->setRange(0, 255);
    m_ui->spinBox_grayHigh->setValue(120);

    m_ui->spinBox_rgb_R_Low->setRange(0, 255);
    m_ui->spinBox_rgb_R_Low->setValue(0);
    m_ui->spinBox_rgb_R_High->setRange(0, 255);
    m_ui->spinBox_rgb_R_High->setValue(120);
    m_ui->spinBox_rgb_G_Low->setRange(0, 255);
    m_ui->spinBox_rgb_G_Low->setValue(0);
    m_ui->spinBox_rgb_G_High->setRange(0, 255);
    m_ui->spinBox_rgb_G_High->setValue(120);
    m_ui->spinBox_rgb_B_Low->setRange(0, 255);
    m_ui->spinBox_rgb_B_Low->setValue(0);
    m_ui->spinBox_rgb_B_High->setRange(0, 255);
    m_ui->spinBox_rgb_B_High->setValue(120);

    m_ui->spinBox_hsv_H_Low->setRange(0, 180);
    m_ui->spinBox_hsv_H_Low->setValue(0);
    m_ui->spinBox_hsv_H_High->setRange(0, 180);
    m_ui->spinBox_hsv_H_High->setValue(120);
    m_ui->spinBox_hsv_S_Low->setRange(0, 255);
    m_ui->spinBox_hsv_S_Low->setValue(0);
    m_ui->spinBox_hsv_S_High->setRange(0, 255);
    m_ui->spinBox_hsv_S_High->setValue(120);
    m_ui->spinBox_hsv_V_Low->setRange(0, 255);
    m_ui->spinBox_hsv_V_Low->setValue(0);
    m_ui->spinBox_hsv_V_High->setRange(0, 255);
    m_ui->spinBox_hsv_V_High->setValue(120);

    setupConnections();

    // 设置防抖定时器
    m_debounceTimer->setSingleShot(true);
    m_debounceTimer->setInterval(50);
    connect(m_debounceTimer, &QTimer::timeout, this, [this]() {
        emit filterConfigChanged();
    });
}

FilterTabWidget::~FilterTabWidget()
{
    m_ui = nullptr;
}

void FilterTabWidget::setupConnections()
{
    // ========== Slider 和 SpinBox 同步 ==========
    // 灰度
    connect(m_ui->Slider_grayLow, &QSlider::valueChanged, m_ui->spinBox_grayLow, &QSpinBox::setValue);
    connect(m_ui->spinBox_grayLow, QOverload<int>::of(&QSpinBox::valueChanged), m_ui->Slider_grayLow, &QSlider::setValue);
    connect(m_ui->Slider_grayHigh, &QSlider::valueChanged, m_ui->spinBox_grayHigh, &QSpinBox::setValue);
    connect(m_ui->spinBox_grayHigh, QOverload<int>::of(&QSpinBox::valueChanged), m_ui->Slider_grayHigh, &QSlider::setValue);

    // RGB
    connect(m_ui->Slider_rgb_R_Low, &QSlider::valueChanged, m_ui->spinBox_rgb_R_Low, &QSpinBox::setValue);
    connect(m_ui->spinBox_rgb_R_Low, QOverload<int>::of(&QSpinBox::valueChanged), m_ui->Slider_rgb_R_Low, &QSlider::setValue);
    connect(m_ui->Slider_rgb_R_High, &QSlider::valueChanged, m_ui->spinBox_rgb_R_High, &QSpinBox::setValue);
    connect(m_ui->spinBox_rgb_R_High, QOverload<int>::of(&QSpinBox::valueChanged), m_ui->Slider_rgb_R_High, &QSlider::setValue);
    connect(m_ui->Slider_rgb_G_Low, &QSlider::valueChanged, m_ui->spinBox_rgb_G_Low, &QSpinBox::setValue);
    connect(m_ui->spinBox_rgb_G_Low, QOverload<int>::of(&QSpinBox::valueChanged), m_ui->Slider_rgb_G_Low, &QSlider::setValue);
    connect(m_ui->Slider_rgb_G_High, &QSlider::valueChanged, m_ui->spinBox_rgb_G_High, &QSpinBox::setValue);
    connect(m_ui->spinBox_rgb_G_High, QOverload<int>::of(&QSpinBox::valueChanged), m_ui->Slider_rgb_G_High, &QSlider::setValue);
    connect(m_ui->Slider_rgb_B_Low, &QSlider::valueChanged, m_ui->spinBox_rgb_B_Low, &QSpinBox::setValue);
    connect(m_ui->spinBox_rgb_B_Low, QOverload<int>::of(&QSpinBox::valueChanged), m_ui->Slider_rgb_B_Low, &QSlider::setValue);
    connect(m_ui->Slider_rgb_B_High, &QSlider::valueChanged, m_ui->spinBox_rgb_B_High, &QSpinBox::setValue);
    connect(m_ui->spinBox_rgb_B_High, QOverload<int>::of(&QSpinBox::valueChanged), m_ui->Slider_rgb_B_High, &QSlider::setValue);

    // HSV
    connect(m_ui->Slider_hsv_H_Low, &QSlider::valueChanged, m_ui->spinBox_hsv_H_Low, &QSpinBox::setValue);
    connect(m_ui->spinBox_hsv_H_Low, QOverload<int>::of(&QSpinBox::valueChanged), m_ui->Slider_hsv_H_Low, &QSlider::setValue);
    connect(m_ui->Slider_hsv_H_High, &QSlider::valueChanged, m_ui->spinBox_hsv_H_High, &QSpinBox::setValue);
    connect(m_ui->spinBox_hsv_H_High, QOverload<int>::of(&QSpinBox::valueChanged), m_ui->Slider_hsv_H_High, &QSlider::setValue);
    connect(m_ui->Slider_hsv_S_Low, &QSlider::valueChanged, m_ui->spinBox_hsv_S_Low, &QSpinBox::setValue);
    connect(m_ui->spinBox_hsv_S_Low, QOverload<int>::of(&QSpinBox::valueChanged), m_ui->Slider_hsv_S_Low, &QSlider::setValue);
    connect(m_ui->Slider_hsv_S_High, &QSlider::valueChanged, m_ui->spinBox_hsv_S_High, &QSpinBox::setValue);
    connect(m_ui->spinBox_hsv_S_High, QOverload<int>::of(&QSpinBox::valueChanged), m_ui->Slider_hsv_S_High, &QSlider::setValue);
    connect(m_ui->Slider_hsv_V_Low, &QSlider::valueChanged, m_ui->spinBox_hsv_V_Low, &QSpinBox::setValue);
    connect(m_ui->spinBox_hsv_V_Low, QOverload<int>::of(&QSpinBox::valueChanged), m_ui->Slider_hsv_V_Low, &QSlider::setValue);
    connect(m_ui->Slider_hsv_V_High, &QSlider::valueChanged, m_ui->spinBox_hsv_V_High, &QSpinBox::setValue);
    connect(m_ui->spinBox_hsv_V_High, QOverload<int>::of(&QSpinBox::valueChanged), m_ui->Slider_hsv_V_High, &QSlider::setValue);

    // ========== 只有 Slider 变化才触发防抖和参数同步 ==========
    // 灰度滑块
    connect(m_ui->Slider_grayLow, &QSlider::valueChanged,
            this, &FilterTabWidget::filterColorChannelsChanged);
    connect(m_ui->Slider_grayHigh, &QSlider::valueChanged,
            this, &FilterTabWidget::filterColorChannelsChanged);

    // RGB 滑块
    connect(m_ui->Slider_rgb_R_Low, &QSlider::valueChanged,
            this, &FilterTabWidget::filterColorChannelsChanged);
    connect(m_ui->Slider_rgb_R_High, &QSlider::valueChanged,
            this, &FilterTabWidget::filterColorChannelsChanged);
    connect(m_ui->Slider_rgb_G_Low, &QSlider::valueChanged,
            this, &FilterTabWidget::filterColorChannelsChanged);
    connect(m_ui->Slider_rgb_G_High, &QSlider::valueChanged,
            this, &FilterTabWidget::filterColorChannelsChanged);
    connect(m_ui->Slider_rgb_B_Low, &QSlider::valueChanged,
            this, &FilterTabWidget::filterColorChannelsChanged);
    connect(m_ui->Slider_rgb_B_High, &QSlider::valueChanged,
            this, &FilterTabWidget::filterColorChannelsChanged);

    // HSV 滑块
    connect(m_ui->Slider_hsv_H_Low, &QSlider::valueChanged,
            this, &FilterTabWidget::filterColorChannelsChanged);
    connect(m_ui->Slider_hsv_H_High, &QSlider::valueChanged,
            this, &FilterTabWidget::filterColorChannelsChanged);
    connect(m_ui->Slider_hsv_S_Low, &QSlider::valueChanged,
            this, &FilterTabWidget::filterColorChannelsChanged);
    connect(m_ui->Slider_hsv_S_High, &QSlider::valueChanged,
            this, &FilterTabWidget::filterColorChannelsChanged);
    connect(m_ui->Slider_hsv_V_Low, &QSlider::valueChanged,
            this, &FilterTabWidget::filterColorChannelsChanged);
    connect(m_ui->Slider_hsv_V_High, &QSlider::valueChanged,
            this, &FilterTabWidget::filterColorChannelsChanged);

    // 过滤模式变化 - 直接连接到槽函数
    connect(m_ui->comboBox_filterMode, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &FilterTabWidget::onFilterModeChanged);
}

void FilterTabWidget::onSelectTypeChanged(int index)
{
    Q_UNUSED(index);
}

void FilterTabWidget::onConditionChanged(int index)
{
    Q_UNUSED(index);
}

void FilterTabWidget::onFilterModeChanged(int index)
{
    switch (index) {
    case 0:  // None
        m_pipelineManager->setGrayFilterEnabled(false);
        m_pipelineManager->setColorFilterEnabled(false);
        m_pipelineManager->setCurrentFilterMode(PipelineConfig::FilterMode::None);
        m_ui->stackedWidget_filter->setCurrentIndex(0);
        break;
    case 1:  // Gray
        m_pipelineManager->setGrayFilterEnabled(true);
        m_pipelineManager->setColorFilterEnabled(false);
        m_pipelineManager->setCurrentFilterMode(PipelineConfig::FilterMode::Gray);
        m_ui->stackedWidget_filter->setCurrentIndex(1);
        syncGrayParameters();
        break;
    case 2:  // RGB
        m_pipelineManager->setColorFilterEnabled(true);
        m_pipelineManager->setColorFilterMode(PipelineConfig::ColorFilterMode::RGB);
        m_pipelineManager->setCurrentFilterMode(PipelineConfig::FilterMode::RGB);
        m_ui->stackedWidget_filter->setCurrentIndex(2);
        syncRGBParameters();
        emit filterConfigChanged();
        return;
    case 3:  // HSV
        m_pipelineManager->setColorFilterEnabled(true);
        m_pipelineManager->setColorFilterMode(PipelineConfig::ColorFilterMode::HSV);
        m_pipelineManager->setCurrentFilterMode(PipelineConfig::FilterMode::HSV);
        m_ui->stackedWidget_filter->setCurrentIndex(3);
        syncHSVParameters();
        emit filterConfigChanged();
        return;
    default:
        m_pipelineManager->setColorFilterEnabled(false);
        m_pipelineManager->setCurrentFilterMode(PipelineConfig::FilterMode::None);
        m_ui->stackedWidget_filter->setCurrentIndex(0);
        break;
    }

    emit filterConfigChanged();
}

void FilterTabWidget::filterColorChannelsChanged()
{
    // 根据当前过滤模式同步参数
    int mode = m_ui->comboBox_filterMode->currentIndex();
    switch (mode) {
    case 1:  // Gray
        syncGrayParameters();
        break;
    case 2:  // RGB
        syncRGBParameters();
        break;
    case 3:  // HSV
        syncHSVParameters();
        break;
    default:
        break;
    }

    m_debounceTimer->start();
}

void FilterTabWidget::syncGrayParameters()
{
    int grayLow = m_ui->Slider_grayLow->value();
    int grayHigh = m_ui->Slider_grayHigh->value();
    m_pipelineManager->getConfig().grayLow = grayLow;
    m_pipelineManager->getConfig().grayHigh = grayHigh;
}

void FilterTabWidget::syncRGBParameters()
{
    int rLow = m_ui->Slider_rgb_R_Low->value();
    int rHigh = m_ui->Slider_rgb_R_High->value();
    int gLow = m_ui->Slider_rgb_G_Low->value();
    int gHigh = m_ui->Slider_rgb_G_High->value();
    int bLow = m_ui->Slider_rgb_B_Low->value();
    int bHigh = m_ui->Slider_rgb_B_High->value();

    m_pipelineManager->setRGBRange(rLow, rHigh, gLow, gHigh, bLow, bHigh);
}

void FilterTabWidget::syncHSVParameters()
{
    int hLow = m_ui->Slider_hsv_H_Low->value();
    int hHigh = m_ui->Slider_hsv_H_High->value();
    int sLow = m_ui->Slider_hsv_S_Low->value();
    int sHigh = m_ui->Slider_hsv_S_High->value();
    int vLow = m_ui->Slider_hsv_V_Low->value();
    int vHigh = m_ui->Slider_hsv_V_High->value();

    m_pipelineManager->setHSVRange(hLow, hHigh, sLow, sHigh, vLow, vHigh);
}
