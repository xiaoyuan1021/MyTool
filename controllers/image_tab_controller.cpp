#include "image_tab_controller.h"
#include <QStatusBar>

ImageTabController::ImageTabController(Ui::MainWindow* ui,
                                       PipelineManager* pipeline,
                                       QObject* parent)
    : QObject(parent)
    , m_ui(ui)
    , m_pipeline(pipeline)
{
}

void ImageTabController::initialize()
{
    connect(m_ui->btn_applyChannel, &QPushButton::clicked,
            this, &ImageTabController::handleApplyChannel);
}

PipelineConfig::Channel ImageTabController::channelFromIndex(int index) const
{
    switch (index)
    {
    case 0: return PipelineConfig::Channel::Gray;
    case 1: return PipelineConfig::Channel::RGB;
    case 2: return PipelineConfig::Channel::HSV;
    case 3: return PipelineConfig::Channel::B;
    case 4: return PipelineConfig::Channel::G;
    case 5: return PipelineConfig::Channel::R;
    default: return PipelineConfig::Channel::RGB;
    }
}

void ImageTabController::handleApplyChannel()
{
    if (!m_ui || !m_pipeline) return;

    int comboIndex = m_ui->comboBox_channels->currentIndex();
    PipelineConfig::Channel channel = channelFromIndex(comboIndex);

    // 设置通道模式到Pipeline
    m_pipeline->setChannelMode(channel);

    emit channelChanged(channel);
}
