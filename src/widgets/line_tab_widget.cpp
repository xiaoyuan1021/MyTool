#include "line_tab_widget.h"
#include "ui_line_tab.h"
#include "logger.h"

LineDetectTabWidget::LineDetectTabWidget(PipelineManager* pipelineManager,
                                         std::function<void()> processCallback,
                                         QWidget* parent)
    : QWidget(parent)
    , m_pipelineManager(pipelineManager)
    , m_processCallback(processCallback)
{
    m_ui = new Ui::LineTabForm();
    m_ui->setupUi(this);
}

LineDetectTabWidget::~LineDetectTabWidget()
{
    delete m_ui;
}

void LineDetectTabWidget::initialize()
{
    // 设置Slider范围和步长
    m_ui->Slider_HoughPThreshold->setRange(1, 200);
    m_ui->Slider_HoughPThreshold->setSingleStep(1);
    m_ui->Slider_HoughPThreshold->setValue(50);

    m_ui->Slider_HoughPMinLineLength->setRange(1, 500);
    m_ui->Slider_HoughPMinLineLength->setSingleStep(1);
    m_ui->Slider_HoughPMinLineLength->setValue(30);

    m_ui->Slider_HoughPMaxLineGap->setRange(0, 100);
    m_ui->Slider_HoughPMaxLineGap->setSingleStep(1);
    m_ui->Slider_HoughPMaxLineGap->setValue(10);

    // 设置SpinBox范围和步长
    m_ui->SpinBox_HoughPThreshold->setRange(1, 200);
    m_ui->SpinBox_HoughPThreshold->setSingleStep(1);
    m_ui->SpinBox_HoughPThreshold->setValue(50);
    
    m_ui->SpinBox_HoughPMinLineLength->setRange(1, 500);
    m_ui->SpinBox_HoughPMinLineLength->setSingleStep(1);
    m_ui->SpinBox_HoughPMinLineLength->setValue(30);

    m_ui->SpinBox_HoughPMaxLineGap->setRange(0, 100);
    m_ui->SpinBox_HoughPMaxLineGap->setSingleStep(1);
    m_ui->SpinBox_HoughPMaxLineGap->setValue(10);

    connect(m_ui->comboBox_lineAlgorithm,&QComboBox::currentIndexChanged,this,&LineDetectTabWidget::onLineAlgorithmChanged);
    connect(m_ui->btn_applyLineDetecter,&QPushButton::clicked,this,&LineDetectTabWidget::handleApply);
    connect(m_ui->Slider_HoughPThreshold,&QSlider::valueChanged,this,&LineDetectTabWidget::onLineThresholdChanged);
    connect(m_ui->Slider_HoughPMinLineLength,&QSlider::valueChanged,this,&LineDetectTabWidget::onLineMinLengthChanged);
    connect(m_ui->Slider_HoughPMaxLineGap,&QSlider::valueChanged,this,&LineDetectTabWidget::onLineMaxGapChanged);

    connect(m_ui->SpinBox_HoughPThreshold, QOverload<int>::of(&QSpinBox::valueChanged), this, &LineDetectTabWidget::onLineThresholdChanged);
    connect(m_ui->SpinBox_HoughPMinLineLength, QOverload<int>::of(&QSpinBox::valueChanged), this, &LineDetectTabWidget::onLineMinLengthChanged);
    connect(m_ui->SpinBox_HoughPMaxLineGap, QOverload<int>::of(&QSpinBox::valueChanged), this, &LineDetectTabWidget::onLineMaxGapChanged);

    // Slider改变时更新SpinBox
    connect(m_ui->Slider_HoughPThreshold, &QSlider::valueChanged,
            m_ui->SpinBox_HoughPThreshold, &QSpinBox::setValue);
    connect(m_ui->Slider_HoughPMinLineLength, &QSlider::valueChanged,
            m_ui->SpinBox_HoughPMinLineLength, &QSpinBox::setValue);
    connect(m_ui->Slider_HoughPMaxLineGap, &QSlider::valueChanged,
            m_ui->SpinBox_HoughPMaxLineGap, &QSpinBox::setValue);

    // SpinBox改变时更新Slider
    connect(m_ui->SpinBox_HoughPThreshold, QOverload<int>::of(&QSpinBox::valueChanged),
            m_ui->Slider_HoughPThreshold, &QSlider::setValue);
    connect(m_ui->SpinBox_HoughPMinLineLength, QOverload<int>::of(&QSpinBox::valueChanged),
            m_ui->Slider_HoughPMinLineLength, &QSlider::setValue);
    connect(m_ui->SpinBox_HoughPMaxLineGap, QOverload<int>::of(&QSpinBox::valueChanged),
            m_ui->Slider_HoughPMaxLineGap, &QSlider::setValue);

}

void LineDetectTabWidget::onLineAlgorithmChanged(int value)
{
    switch (value) 
    {
    case 0: // HoughP
        m_ui->stackedWidget_LineDetect->setCurrentIndex(1);
        break;
    case 1: // LSD
        m_ui->stackedWidget_LineDetect->setCurrentIndex(0);
        break;
    case 2: // EDline
        m_ui->stackedWidget_LineDetect->setCurrentIndex(0);
        break;
    default:
        m_ui->stackedWidget_LineDetect->setCurrentIndex(0);
    }
}

void LineDetectTabWidget::onLineThresholdChanged(int value)
{
    Q_UNUSED(value);
    if (m_pipelineManager && m_pipelineManager->getConfig().enableLineDetect) {
        if (m_processCallback) {
            m_processCallback();
        }
    }
}

void LineDetectTabWidget::onLineMinLengthChanged(int value)
{
    Q_UNUSED(value);
    if (m_pipelineManager && m_pipelineManager->getConfig().enableLineDetect) {
        if (m_processCallback) {
            m_processCallback();
        }
    }
}

void LineDetectTabWidget::onLineMaxGapChanged(int value)
{
    Q_UNUSED(value);
    if (m_pipelineManager && m_pipelineManager->getConfig().enableLineDetect) {
        if (m_processCallback) {
            m_processCallback();
        }
    }
}

void LineDetectTabWidget::handleApply()
{
    if (!m_pipelineManager) return;

    LineDetectState state = getLineState();

    // 更新Pipeline配置
    m_pipelineManager->getConfig().lineDetectAlgorithm = state.algorithm;
    m_pipelineManager->getConfig().lineThreshold = state.threshold;
    m_pipelineManager->getConfig().lineMinLength = state.minLineLength;
    m_pipelineManager->getConfig().lineMaxGap = state.maxLineGap;
    m_pipelineManager->getConfig().enableLineDetect = true;

    // 设置显示模式为LineDetect
    m_pipelineManager->setDisplayMode(DisplayConfig::Mode::LineDetect);

    if (m_processCallback)
    {
        m_processCallback();
    }
}

LineDetectTabWidget::LineDetectState LineDetectTabWidget::getLineState() const
{
    LineDetectState state;
    state.algorithm = m_ui->comboBox_lineAlgorithm->currentIndex();
    state.threshold = m_ui->Slider_HoughPThreshold->value();
    state.minLineLength = m_ui->Slider_HoughPMinLineLength->value();
    state.maxLineGap = m_ui->Slider_HoughPMaxLineGap->value();
    return state;
}

void LineDetectTabWidget::setLineState(const LineDetectState &state)
{
    const QSignalBlocker b1(m_ui->Slider_HoughPThreshold);
    const QSignalBlocker b2(m_ui->Slider_HoughPMinLineLength);
    const QSignalBlocker b3(m_ui->Slider_HoughPMaxLineGap);
    const QSignalBlocker sb1(m_ui->SpinBox_HoughPThreshold);
    const QSignalBlocker sb2(m_ui->SpinBox_HoughPMinLineLength);
    const QSignalBlocker sb3(m_ui->SpinBox_HoughPMaxLineGap);

    m_ui->comboBox_lineAlgorithm->setCurrentIndex(state.algorithm);
    m_ui->Slider_HoughPThreshold->setValue(state.threshold);
    m_ui->Slider_HoughPMinLineLength->setValue(state.minLineLength);
    m_ui->Slider_HoughPMaxLineGap->setValue(state.maxLineGap);

    m_ui->SpinBox_HoughPThreshold->setValue(state.threshold);
    m_ui->SpinBox_HoughPMinLineLength->setValue(state.minLineLength);
    m_ui->SpinBox_HoughPMaxLineGap->setValue(state.maxLineGap);
}