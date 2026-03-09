#include "line_detect_tab_controller.h"
#include "logger.h"
LineDetectTabController::LineDetectTabController(Ui::MainWindow *ui, PipelineManager *pipeline, std::function<void()> processCallback,QObject *parent)
    : QObject(parent), 
    m_ui(ui), 
    m_pipeline(pipeline),
    m_processCallback(processCallback)
{

}

void LineDetectTabController::initialize()
{
    // 设置Slider范围和步长
    m_ui->Slider_LSDThreshold->setRange(1, 200);
    m_ui->Slider_LSDThreshold->setSingleStep(1);
    m_ui->Slider_LSDThreshold->setValue(50);
    
    m_ui->Slider_LSDMinLineLength->setRange(1, 500);
    m_ui->Slider_LSDMinLineLength->setSingleStep(1);
    m_ui->Slider_LSDMinLineLength->setValue(30);
    
    m_ui->Slider_LSDMaxLineGap->setRange(0, 100);
    m_ui->Slider_LSDMaxLineGap->setSingleStep(1);
    m_ui->Slider_LSDMaxLineGap->setValue(10);
    
    // 设置SpinBox范围和步长
    m_ui->SpinBox_LSDThreshold->setRange(1, 200);
    m_ui->SpinBox_LSDThreshold->setSingleStep(1);
    m_ui->SpinBox_LSDThreshold->setValue(50);
    
    m_ui->SpinBox_LSDMinLineLength->setRange(1, 500);
    m_ui->SpinBox_LSDMinLineLength->setSingleStep(1);
    m_ui->SpinBox_LSDMinLineLength->setValue(30);
    
    m_ui->SpinBox_LSDMaxLineGap->setRange(0, 100);
    m_ui->SpinBox_LSDMaxLineGap->setSingleStep(1);
    m_ui->SpinBox_LSDMaxLineGap->setValue(10);

    connect(m_ui->comboBox_lineAlgorithm,&QComboBox::currentIndexChanged,this,&LineDetectTabController::onLineAlgorithmChanged);
    connect(m_ui->btn_applyLineDetecter,&QPushButton::clicked,this,&LineDetectTabController::handleApply);
    connect(m_ui->Slider_LSDThreshold,&QSlider::valueChanged,this,&LineDetectTabController::onLineThresholdChanged);
    connect(m_ui->Slider_LSDMinLineLength,&QSlider::valueChanged,this,&LineDetectTabController::onLineMinLengthChanged);
    connect(m_ui->Slider_LSDMaxLineGap,&QSlider::valueChanged,this,&LineDetectTabController::onLineMaxGapChanged);
    
    connect(m_ui->SpinBox_LSDThreshold, QOverload<int>::of(&QSpinBox::valueChanged), this, &LineDetectTabController::onLineThresholdChanged);
    connect(m_ui->SpinBox_LSDMinLineLength, QOverload<int>::of(&QSpinBox::valueChanged), this, &LineDetectTabController::onLineMinLengthChanged);
    connect(m_ui->SpinBox_LSDMaxLineGap, QOverload<int>::of(&QSpinBox::valueChanged), this, &LineDetectTabController::onLineMaxGapChanged);

    // Slider改变时更新SpinBox
    connect(m_ui->Slider_LSDThreshold, &QSlider::valueChanged, 
            m_ui->SpinBox_LSDThreshold, &QSpinBox::setValue);
    connect(m_ui->Slider_LSDMinLineLength, &QSlider::valueChanged, 
            m_ui->SpinBox_LSDMinLineLength, &QSpinBox::setValue);
    connect(m_ui->Slider_LSDMaxLineGap, &QSlider::valueChanged, 
            m_ui->SpinBox_LSDMaxLineGap, &QSpinBox::setValue);

    // SpinBox改变时更新Slider
    connect(m_ui->SpinBox_LSDThreshold, QOverload<int>::of(&QSpinBox::valueChanged), 
            m_ui->Slider_LSDThreshold, &QSlider::setValue);
    connect(m_ui->SpinBox_LSDMinLineLength, QOverload<int>::of(&QSpinBox::valueChanged), 
            m_ui->Slider_LSDMinLineLength, &QSlider::setValue);
    connect(m_ui->SpinBox_LSDMaxLineGap, QOverload<int>::of(&QSpinBox::valueChanged), 
            m_ui->Slider_LSDMaxLineGap, &QSlider::setValue);

}

void LineDetectTabController::onLineAlgorithmChanged(int value)
{
    Q_UNUSED(value);
}

void LineDetectTabController::onLineThresholdChanged(int value)
{
    Q_UNUSED(value);
    if (m_pipeline && m_pipeline->getConfig().enableLineDetect) {
        if (m_processCallback) {
            m_processCallback();
        }
    }
}

void LineDetectTabController::onLineMinLengthChanged(int value)
{
    Q_UNUSED(value);
    if (m_pipeline && m_pipeline->getConfig().enableLineDetect) {
        if (m_processCallback) {
            m_processCallback();
        }
    }
}

void LineDetectTabController::onLineMaxGapChanged(int value)
{
    Q_UNUSED(value);
    if (m_pipeline && m_pipeline->getConfig().enableLineDetect) {
        if (m_processCallback) {
            m_processCallback();
        }
    }
}

void LineDetectTabController::handleApply()
{
    if (!m_pipeline) return;

    LineDetectState state = getLineState();

    // 更新Pipeline配置
    m_pipeline->getConfig().lineDetectAlgorithm = state.algorithm;
    m_pipeline->getConfig().lineThreshold = state.threshold;
    m_pipeline->getConfig().lineMinLength = state.minLineLength;
    m_pipeline->getConfig().lineMaxGap = state.maxLineGap;
    m_pipeline->getConfig().enableLineDetect = true;

    if (m_processCallback) 
    {
        m_processCallback();
    }
}

LineDetectTabController::LineDetectState LineDetectTabController::getLineState() const
{
    LineDetectState state;
    state.algorithm = m_ui->comboBox_lineAlgorithm->currentIndex();
    state.threshold = m_ui->Slider_LSDThreshold->value();
    state.minLineLength = m_ui->Slider_LSDMinLineLength->value();
    state.maxLineGap = m_ui->Slider_LSDMaxLineGap->value();
    return state;
}

void LineDetectTabController::setLineState(const LineDetectState &state)
{
    const QSignalBlocker b1(m_ui->Slider_LSDThreshold);
    const QSignalBlocker b2(m_ui->Slider_LSDMinLineLength);
    const QSignalBlocker b3(m_ui->Slider_LSDMaxLineGap);
    const QSignalBlocker sb1(m_ui->SpinBox_LSDThreshold);
    const QSignalBlocker sb2(m_ui->SpinBox_LSDMinLineLength);
    const QSignalBlocker sb3(m_ui->SpinBox_LSDMaxLineGap);
    
    m_ui->comboBox_lineAlgorithm->setCurrentIndex(state.algorithm);
    m_ui->Slider_LSDThreshold->setValue(state.threshold);
    m_ui->Slider_LSDMinLineLength->setValue(state.minLineLength);
    m_ui->Slider_LSDMaxLineGap->setValue(state.maxLineGap);
    
    m_ui->SpinBox_LSDThreshold->setValue(state.threshold);
    m_ui->SpinBox_LSDMinLineLength->setValue(state.minLineLength);
    m_ui->SpinBox_LSDMaxLineGap->setValue(state.maxLineGap);
}
