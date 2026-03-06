#include "filter_tab_controller.h"

FilterTabController::FilterTabController(Ui::MainWindow* ui,
                                         PipelineManager* pipeline,
                                         QObject* parent)
    : QObject(parent),
      m_ui(ui),
      m_pipeline(pipeline),
      m_debounceTimer(new QTimer(this))
{
}

void FilterTabController::initialize()
{
    setupConnections();

    // 设置防抖定时器
    m_debounceTimer->setSingleShot(true);
    m_debounceTimer->setInterval(150);

    // 连接过滤模式变化
    connect(m_ui->comboBox_filterMode, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &FilterTabController::onFilterModeUIChanged);
}

void FilterTabController::setupConnections()
{
    // ========== 颜色通道滑块连接 ==========
    // 灰度滑块
    connect(m_ui->Slider_grayLow, &QSlider::valueChanged,
            this, &FilterTabController::filterColorChannelsChanged);
    connect(m_ui->Slider_grayHigh, &QSlider::valueChanged,
            this, &FilterTabController::filterColorChannelsChanged);

    // RGB 滑块
    connect(m_ui->Slider_rgb_R_Low, &QSlider::valueChanged,
            this, &FilterTabController::filterColorChannelsChanged);
    connect(m_ui->Slider_rgb_R_High, &QSlider::valueChanged,
            this, &FilterTabController::filterColorChannelsChanged);
    connect(m_ui->Slider_rgb_G_Low, &QSlider::valueChanged,
            this, &FilterTabController::filterColorChannelsChanged);
    connect(m_ui->Slider_rgb_G_High, &QSlider::valueChanged,
            this, &FilterTabController::filterColorChannelsChanged);
    connect(m_ui->Slider_rgb_B_Low, &QSlider::valueChanged,
            this, &FilterTabController::filterColorChannelsChanged);
    connect(m_ui->Slider_rgb_B_High, &QSlider::valueChanged,
            this, &FilterTabController::filterColorChannelsChanged);

    // HSV 滑块
    connect(m_ui->Slider_hsv_H_Low, &QSlider::valueChanged,
            this, &FilterTabController::filterColorChannelsChanged);
    connect(m_ui->Slider_hsv_H_High, &QSlider::valueChanged,
            this, &FilterTabController::filterColorChannelsChanged);
    connect(m_ui->Slider_hsv_S_Low, &QSlider::valueChanged,
            this, &FilterTabController::filterColorChannelsChanged);
    connect(m_ui->Slider_hsv_S_High, &QSlider::valueChanged,
            this, &FilterTabController::filterColorChannelsChanged);
    connect(m_ui->Slider_hsv_V_Low, &QSlider::valueChanged,
            this, &FilterTabController::filterColorChannelsChanged);
    connect(m_ui->Slider_hsv_V_High, &QSlider::valueChanged,
            this, &FilterTabController::filterColorChannelsChanged);
}

void FilterTabController::filterColorChannelsChanged()
{
    //m_debounceTimer->start(150);
    emit filterConfigChanged();
}

void FilterTabController::handleFilterModeChanged(int index)
{
    configureColorFilter(index);
    emit filterConfigChanged();
}

void FilterTabController::configureColorFilter(int filterModeIndex)
{
    PipelineConfig::FilterMode targetMode;

    switch (filterModeIndex) {
    case 0:  // None
        targetMode = PipelineConfig::FilterMode::None;
        m_pipeline->setGrayFilterEnabled(false);
        m_pipeline->setColorFilterEnabled(false);
        break;
    case 1:  // Gray
        targetMode = PipelineConfig::FilterMode::Gray;
        m_pipeline->setGrayFilterEnabled(true);
        m_pipeline->setColorFilterEnabled(false);
        break;
    case 2:  // RGB
        targetMode = PipelineConfig::FilterMode::RGB;
        m_pipeline->setColorFilterEnabled(true);
        m_pipeline->setColorFilterMode(PipelineConfig::ColorFilterMode::RGB);
        syncRGBParameters();
        break;
    case 3:  // HSV
        targetMode = PipelineConfig::FilterMode::HSV;
        m_pipeline->setColorFilterEnabled(true);
        m_pipeline->setColorFilterMode(PipelineConfig::ColorFilterMode::HSV);
        syncHSVParameters();
        break;
    default:
        targetMode = PipelineConfig::FilterMode::None;
        m_pipeline->setColorFilterEnabled(false);
        break;
    }

    m_pipeline->setCurrentFilterMode(targetMode);
}

void FilterTabController::syncRGBParameters()
{
    m_pipeline->setRGBRange(
        m_ui->Slider_rgb_R_Low->value(),
        m_ui->Slider_rgb_R_High->value(),
        m_ui->Slider_rgb_G_Low->value(),
        m_ui->Slider_rgb_G_High->value(),
        m_ui->Slider_rgb_B_Low->value(),
        m_ui->Slider_rgb_B_High->value()
    );
}

void FilterTabController::syncHSVParameters()
{
    m_pipeline->setHSVRange(
        m_ui->Slider_hsv_H_Low->value(),
        m_ui->Slider_hsv_H_High->value(),
        m_ui->Slider_hsv_S_Low->value(),
        m_ui->Slider_hsv_S_High->value(),
        m_ui->Slider_hsv_V_Low->value(),
        m_ui->Slider_hsv_V_High->value()
    );
}

void FilterTabController::onFilterModeUIChanged(int index)
{
    switch (index) {
    case 0: m_ui->stackedWidget_filter->setCurrentIndex(0); break;
    case 1: m_ui->stackedWidget_filter->setCurrentIndex(1); break;
    case 2: m_ui->stackedWidget_filter->setCurrentIndex(2); break;
    case 3: m_ui->stackedWidget_filter->setCurrentIndex(3); break;
    default: m_ui->stackedWidget_filter->setCurrentIndex(0); break;
    }
    configureColorFilter(index);
    emit filterConfigChanged();
}
