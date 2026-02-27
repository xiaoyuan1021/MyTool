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
    case 0: return PipelineConfig::Channel::RGB;
    case 1: return PipelineConfig::Channel::Gray;
    case 2: return PipelineConfig::Channel::B;
    case 3: return PipelineConfig::Channel::G;
    case 4: return PipelineConfig::Channel::R;
    default: return PipelineConfig::Channel::RGB;
    }
}

void ImageTabController::handleApplyChannel()
{
    if (!m_ui || !m_pipeline) return;

    int comboIndex = m_ui->comboBox_channels->currentIndex();
    PipelineConfig::Channel channel = channelFromIndex(comboIndex);
    m_pipeline->setChannelMode(channel);

    m_channelFlag = !m_channelFlag;
    m_ui->btn_applyChannel->setText(
        m_channelFlag ? "通道切换: ON" : "通道切换: OFF");

    QString tip = m_channelFlag
        ? "已应用通道效果"
        : "已取消通道效果";
    if (m_ui->statusbar) m_ui->statusbar->showMessage(tip, 2000);

    emit channelChanged(channel);   // 交给 MainWindow 触发刷新
}
