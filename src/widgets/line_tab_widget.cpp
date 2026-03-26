#include "line_tab_widget.h"
#include "ui_line_tab.h"
#include "logger.h"
#include <QMessageBox>

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

    // ========== 参考线匹配信号连接 ==========
    connect(m_ui->chk_enableReferenceLine, &QCheckBox::toggled, this, &LineDetectTabWidget::onReferenceLineEnabledChanged);
    connect(m_ui->btn_drawReferenceLine, &QPushButton::clicked, this, &LineDetectTabWidget::onDrawReferenceLineClicked);
    connect(m_ui->btn_clearReferenceLine, &QPushButton::clicked, this, &LineDetectTabWidget::onClearReferenceLineClicked);
    connect(m_ui->spin_angleThreshold, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &LineDetectTabWidget::onAngleThresholdChanged);
    connect(m_ui->spin_distanceThreshold, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &LineDetectTabWidget::onDistanceThresholdChanged);
    connect(m_ui->spin_searchRegionWidth, QOverload<int>::of(&QSpinBox::valueChanged), this, &LineDetectTabWidget::onSearchRegionWidthChanged);

    // 初始化参考线匹配控件状态
    m_ui->groupBox_referenceLine->setEnabled(true);
    m_ui->spin_angleThreshold->setValue(15.0);
    m_ui->spin_distanceThreshold->setValue(50.0);
    m_ui->spin_searchRegionWidth->setValue(100);
    updateReferenceLineStatus();

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
    case 3: // EdgesSubPix(Halcon)
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
    
    // 判断是否启用参考线匹配模式
    bool useReferenceLineMatch = m_pipelineManager->getConfig().enableReferenceLineMatch && 
                                  m_pipelineManager->getConfig().referenceLineValid;
    
    if (useReferenceLineMatch) {
        // 参考线匹配模式：禁用普通直线检测
        m_pipelineManager->getConfig().enableLineDetect = false;
        Logger::instance()->info("使用参考线匹配模式检测直线");
    } else {
        // 普通直线检测模式：禁用参考线匹配
        m_pipelineManager->getConfig().enableLineDetect = true;
        m_pipelineManager->getConfig().enableReferenceLineMatch = false;
        Logger::instance()->info("使用普通直线检测模式");
    }

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

// ========== 参考线匹配槽函数 ==========

void LineDetectTabWidget::onReferenceLineEnabledChanged(bool enabled)
{
    if (!m_pipelineManager) return;

    m_pipelineManager->getConfig().enableReferenceLineMatch = enabled;

    // 如果启用参考线匹配，禁用普通直线检测
    if (enabled) {
        m_pipelineManager->getConfig().enableLineDetect = false;
    }

    updateReferenceLineStatus();

    if (m_processCallback) {
        m_processCallback();
    }
}

void LineDetectTabWidget::onDrawReferenceLineClicked()
{
    // 发射信号请求绘制参考线
    emit requestDrawReferenceLine();
    
    // 提示用户（不弹窗，只在日志显示）
    Logger::instance()->info("请在图像上点击绘制参考线的起点和终点");
}

void LineDetectTabWidget::onClearReferenceLineClicked()
{
    if (!m_pipelineManager) return;

    // 清除参考线数据
    m_pipelineManager->getConfig().referenceLineValid = false;
    m_pipelineManager->getConfig().referenceLineStart = cv::Point2f(0, 0);
    m_pipelineManager->getConfig().referenceLineEnd = cv::Point2f(0, 0);

    updateReferenceLineStatus();

    // 发射信号请求清除绘制
    emit requestClearReferenceLine();

    if (m_processCallback) {
        m_processCallback();
    }
}

void LineDetectTabWidget::onAngleThresholdChanged(double value)
{
    if (!m_pipelineManager) return;

    m_pipelineManager->getConfig().angleThreshold = value;

    if (m_pipelineManager->getConfig().enableReferenceLineMatch && 
        m_pipelineManager->getConfig().referenceLineValid) {
        if (m_processCallback) {
            m_processCallback();
        }
    }
}

void LineDetectTabWidget::onDistanceThresholdChanged(double value)
{
    if (!m_pipelineManager) return;

    m_pipelineManager->getConfig().distanceThreshold = value;

    if (m_pipelineManager->getConfig().enableReferenceLineMatch && 
        m_pipelineManager->getConfig().referenceLineValid) {
        if (m_processCallback) {
            m_processCallback();
        }
    }
}

void LineDetectTabWidget::onSearchRegionWidthChanged(int value)
{
    if (!m_pipelineManager) return;

    m_pipelineManager->getConfig().searchRegionWidth = value;

    if (m_pipelineManager->getConfig().enableReferenceLineMatch && 
        m_pipelineManager->getConfig().referenceLineValid) {
        if (m_processCallback) {
            m_processCallback();
        }
    }
}

void LineDetectTabWidget::updateReferenceLineStatus()
{
    if (!m_pipelineManager || !m_ui) return;

    const PipelineConfig& cfg = m_pipelineManager->getConfig();
    QString status;

    if (!cfg.enableReferenceLineMatch) {
        status = "状态：参考线匹配未启用";
    } else if (!cfg.referenceLineValid) {
        status = "状态：参考线未绘制\n点击\"绘制参考线\"按钮开始";
    } else {
        status = QString("状态：参考线已绘制\n起点:(%1,%2)\n终点:(%3,%4)")
                     .arg(cfg.referenceLineStart.x, 0, 'f', 1)
                     .arg(cfg.referenceLineStart.y, 0, 'f', 1)
                     .arg(cfg.referenceLineEnd.x, 0, 'f', 1)
                     .arg(cfg.referenceLineEnd.y, 0, 'f', 1);
    }

    m_ui->label_referenceLineStatus->setText(status);
}

void LineDetectTabWidget::setReferenceLine(const cv::Point2f& start, const cv::Point2f& end)
{
    if (!m_pipelineManager) return;

    m_pipelineManager->getConfig().referenceLineStart = start;
    m_pipelineManager->getConfig().referenceLineEnd = end;
    m_pipelineManager->getConfig().referenceLineValid = true;

    updateReferenceLineStatus();

    // 自动启用参考线匹配
    m_ui->chk_enableReferenceLine->setChecked(true);

    if (m_processCallback) {
        m_processCallback();
    }
}

void LineDetectTabWidget::updateMatchResultStatus(int matchedCount, int totalCount, double angleThreshold, double distanceThreshold)
{
    if (!m_ui) return;

    QString status;
    
    if (matchedCount > 0) {
        status = QString("匹配结果: %1/%2 条直线匹配成功\n角度容差: %3°, 距离容差: %4px")
                     .arg(matchedCount)
                     .arg(totalCount)
                     .arg(angleThreshold)
                     .arg(distanceThreshold);
    } else {
        status = QString("匹配结果: 未找到匹配直线\n角度容差: %1°, 距离容差: %2px")
                     .arg(angleThreshold)
                     .arg(distanceThreshold);
    }

    m_ui->label_referenceLineStatus->setText(status);
}
