#include "image_tab_widget.h"
#include "ui_image_tab.h"

ImageTabWidget::ImageTabWidget(PipelineManager* pipelineManager, QWidget* parent)
    : QWidget(parent)
    , m_ui(new Ui::ImageTabWidget)
    , m_pipelineManager(pipelineManager)
{
    m_ui->setupUi(this);
    // Qt 会自动连接 on_btn_applyChannel_clicked 和 on_comboBox_channels_currentIndexChanged
    // 因为它们被声明为 private slots，无需手动 connect
}

ImageTabWidget::~ImageTabWidget()
{
    // m_ui 的所有权由 Qt 管理，不需要手动 delete
    // 只需要将指针置空
    m_ui = nullptr;
}

PipelineConfig::Channel ImageTabWidget::channelFromIndex(int index) const
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

void ImageTabWidget::on_btn_applyChannel_clicked()
{
    if (!m_ui || !m_pipelineManager) return;

    int comboIndex = m_ui->comboBox_channels->currentIndex();
    PipelineConfig::Channel channel = channelFromIndex(comboIndex);

    // 设置通道模式到Pipeline
    m_pipelineManager->setChannelMode(channel);

    emit channelChanged(comboIndex);
}

void ImageTabWidget::on_comboBox_channels_currentIndexChanged(int index)
{
    // 仅当下拉框索引改变时记录，但不发送信号
    // 信号只由按钮点击触发，避免重复
    
}

