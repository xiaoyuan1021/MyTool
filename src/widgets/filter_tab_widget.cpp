#include "filter_tab_widget.h"
#include "ui_filter_tab.h"
#include "logger.h"
#include "config/constants.h"
#include "config/config_manager.h"
#include "ui/slider_spinbox_binder.h"
#include <QDebug>

FilterTabWidget::FilterTabWidget(PipelineManager* pipelineManager, QWidget* parent)
    : QWidget(parent)
    , m_ui(new Ui::FilterTabWidget)
    , m_pipelineManager(pipelineManager)
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
}

FilterTabWidget::~FilterTabWidget()
{
    delete m_ui;
}

void FilterTabWidget::setupConnections()
{
    // ========== Slider ↔ SpinBox 双向同步 ==========
    bindSliderAndSpinBox(m_ui->Slider_grayLow, m_ui->spinBox_grayLow);
    bindSliderAndSpinBox(m_ui->Slider_grayHigh, m_ui->spinBox_grayHigh);
    bindSliderAndSpinBox(m_ui->Slider_rgb_R_Low, m_ui->spinBox_rgb_R_Low);
    bindSliderAndSpinBox(m_ui->Slider_rgb_R_High, m_ui->spinBox_rgb_R_High);
    bindSliderAndSpinBox(m_ui->Slider_rgb_G_Low, m_ui->spinBox_rgb_G_Low);
    bindSliderAndSpinBox(m_ui->Slider_rgb_G_High, m_ui->spinBox_rgb_G_High);
    bindSliderAndSpinBox(m_ui->Slider_rgb_B_Low, m_ui->spinBox_rgb_B_Low);
    bindSliderAndSpinBox(m_ui->Slider_rgb_B_High, m_ui->spinBox_rgb_B_High);
    bindSliderAndSpinBox(m_ui->Slider_hsv_H_Low, m_ui->spinBox_hsv_H_Low);
    bindSliderAndSpinBox(m_ui->Slider_hsv_H_High, m_ui->spinBox_hsv_H_High);
    bindSliderAndSpinBox(m_ui->Slider_hsv_S_Low, m_ui->spinBox_hsv_S_Low);
    bindSliderAndSpinBox(m_ui->Slider_hsv_S_High, m_ui->spinBox_hsv_S_High);
    bindSliderAndSpinBox(m_ui->Slider_hsv_V_Low, m_ui->spinBox_hsv_V_Low);
    bindSliderAndSpinBox(m_ui->Slider_hsv_V_High, m_ui->spinBox_hsv_V_High);

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
    auto& cf = m_pipelineManager->mutableConfig().colorFilter;
    switch (index) {
    case 0:  // None
        cf.enableGrayFilter = false;
        cf.enableColorFilter = false;
        cf.currentFilterMode = ImageFilterMode::None;
        m_ui->stackedWidget_filter->setCurrentIndex(0);
        break;
    case 1:  // Gray
        cf.enableGrayFilter = true;
        cf.enableColorFilter = false;
        cf.currentFilterMode = ImageFilterMode::Gray;
        m_ui->stackedWidget_filter->setCurrentIndex(1);
        syncGrayParameters();
        break;
    case 2:  // RGB
        cf.enableColorFilter = true;
        cf.colorFilterMode = ColorFilterMode::RGB;
        cf.currentFilterMode = ImageFilterMode::RGB;
        m_ui->stackedWidget_filter->setCurrentIndex(2);
        syncRGBParameters();
        emit filterConfigChanged();
        return;
    case 3:  // HSV
        cf.enableColorFilter = true;
        cf.colorFilterMode = ColorFilterMode::HSV;
        cf.currentFilterMode = ImageFilterMode::HSV;
        m_ui->stackedWidget_filter->setCurrentIndex(3);
        syncHSVParameters();
        emit filterConfigChanged();
        return;
    default:
        cf.enableColorFilter = false;
        cf.currentFilterMode = ImageFilterMode::None;
        m_ui->stackedWidget_filter->setCurrentIndex(0);
        break;
    }

    emit filterConfigChanged();
}

void FilterTabWidget::filterColorChannelsChanged()
{
    // 总是同步所有参数，而不是只同步当前模式
    // 这样用户可以在任何模式下调整滑条并立即看到效果
    syncGrayParameters();
    syncRGBParameters();
    syncHSVParameters();

    // 直接发出信号，消抖由 PipelineScheduler 统一处理
    emit filterConfigChanged();
}

void FilterTabWidget::syncGrayParameters()
{
    int grayLow = m_ui->Slider_grayLow->value();
    int grayHigh = m_ui->Slider_grayHigh->value();
    auto& cf = m_pipelineManager->mutableConfig().colorFilter;
    cf.grayLow = grayLow;
    cf.grayHigh = grayHigh;
}

void FilterTabWidget::syncRGBParameters()
{
    int rLow = m_ui->Slider_rgb_R_Low->value();
    int rHigh = m_ui->Slider_rgb_R_High->value();
    int gLow = m_ui->Slider_rgb_G_Low->value();
    int gHigh = m_ui->Slider_rgb_G_High->value();
    int bLow = m_ui->Slider_rgb_B_Low->value();
    int bHigh = m_ui->Slider_rgb_B_High->value();

    auto& cf = m_pipelineManager->mutableConfig().colorFilter;
    cf.rLow = rLow; cf.rHigh = rHigh;
    cf.gLow = gLow; cf.gHigh = gHigh;
    cf.bLow = bLow; cf.bHigh = bHigh;
}

void FilterTabWidget::syncHSVParameters()
{
    int hLow = m_ui->Slider_hsv_H_Low->value();
    int hHigh = m_ui->Slider_hsv_H_High->value();
    int sLow = m_ui->Slider_hsv_S_Low->value();
    int sHigh = m_ui->Slider_hsv_S_High->value();
    int vLow = m_ui->Slider_hsv_V_Low->value();
    int vHigh = m_ui->Slider_hsv_V_High->value();

    auto& cf = m_pipelineManager->mutableConfig().colorFilter;
    cf.hLow = hLow; cf.hHigh = hHigh;
    cf.sLow = sLow; cf.sHigh = sHigh;
    cf.vLow = vLow; cf.vHigh = vHigh;
}

void FilterTabWidget::getFilterConfig(int& mode, int& grayLow, int& grayHigh) const
{
    mode = m_ui->comboBox_filterMode->currentIndex();
    grayLow = m_ui->Slider_grayLow->value();
    grayHigh = m_ui->Slider_grayHigh->value();
}

void FilterTabWidget::setFilterConfig(int mode, int grayLow, int grayHigh)
{
    const QSignalBlocker b1(m_ui->Slider_grayLow);
    const QSignalBlocker b2(m_ui->Slider_grayHigh);
    const QSignalBlocker sb1(m_ui->spinBox_grayLow);
    const QSignalBlocker sb2(m_ui->spinBox_grayHigh);

    m_ui->comboBox_filterMode->setCurrentIndex(mode);
    m_ui->Slider_grayLow->setValue(grayLow);
    m_ui->Slider_grayHigh->setValue(grayHigh);
    m_ui->spinBox_grayLow->setValue(grayLow);
    m_ui->spinBox_grayHigh->setValue(grayHigh);
}

void FilterTabWidget::connectSignals(PipelineManager* pm, RoiManager* rm,
                                     ImageView* view, RoiUiController* roiCtrl,
                                     std::function<void()> onConfigChanged,
                                     std::function<void()> onExecuteRequested)
{
    Q_UNUSED(pm); Q_UNUSED(rm); Q_UNUSED(view); Q_UNUSED(roiCtrl);
    connect(this, &FilterTabWidget::filterConfigChanged,
            this, [onConfigChanged]() { onConfigChanged(); });
}

// ========== IConfigurableTab 接口实现 ==========

void FilterTabWidget::saveToConfig(PipelineConfig& config) const
{
    int mode, grayLow, grayHigh;
    getFilterConfig(mode, grayLow, grayHigh);
    config.colorFilter.currentFilterMode = static_cast<ImageFilterMode>(mode);
    config.colorFilter.grayLow = grayLow;
    config.colorFilter.grayHigh = grayHigh;
}

void FilterTabWidget::loadFromConfig(const PipelineConfig& config)
{
    setFilterConfig(
        static_cast<int>(config.colorFilter.currentFilterMode),
        config.colorFilter.grayLow,
        config.colorFilter.grayHigh
    );
}
