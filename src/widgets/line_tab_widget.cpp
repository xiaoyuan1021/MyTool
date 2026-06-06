#include "line_tab_widget.h"
#include "ui_line_tab.h"
#include "logger.h"
#include "ui/slider_spinbox_binder.h"
#include "image_view.h"
#include "controllers/roi_ui_controller.h"
#include <QMessageBox>

LineDetectTabWidget::LineDetectTabWidget(PipelineManager* pipelineManager,
                                         QWidget* parent)
    : QWidget(parent)
    , m_pipelineManager(pipelineManager)
{
    m_ui = new Ui::LineTabForm();
    m_ui->setupUi(this);
}

LineDetectTabWidget::~LineDetectTabWidget()
{
    delete m_ui;
}

void LineDetectTabWidget::updateFromPipeline(const PipelineContext& ctx)
{
    PipelineConfig cfg = m_pipelineManager->getConfigSnapshot();
    updateMatchResultStatus(
        ctx.matchedLineCount,
        ctx.totalLineCount,
        cfg.lineDetect.angleThreshold,
        cfg.lineDetect.distanceThreshold,
        cfg.lineDetect.enableReferenceLineMatch
    );
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

    // Slider变化时触发Pipeline（SpinBox通过bindSliderAndSpinBox同步值，不重复连接）
    connect(m_ui->Slider_HoughPThreshold,&QSlider::valueChanged,this,&LineDetectTabWidget::onLineThresholdChanged);
    connect(m_ui->Slider_HoughPMinLineLength,&QSlider::valueChanged,this,&LineDetectTabWidget::onLineMinLengthChanged);
    connect(m_ui->Slider_HoughPMaxLineGap,&QSlider::valueChanged,this,&LineDetectTabWidget::onLineMaxGapChanged);

    // SpinBox直接输入时也触发Pipeline（用信号阻断防止Slider回调时重复触发）
    connect(m_ui->SpinBox_HoughPThreshold, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int value) {
        const QSignalBlocker b(m_ui->Slider_HoughPThreshold);
        onLineThresholdChanged(value);
    });
    connect(m_ui->SpinBox_HoughPMinLineLength, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int value) {
        const QSignalBlocker b(m_ui->Slider_HoughPMinLineLength);
        onLineMinLengthChanged(value);
    });
    connect(m_ui->SpinBox_HoughPMaxLineGap, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int value) {
        const QSignalBlocker b(m_ui->Slider_HoughPMaxLineGap);
        onLineMaxGapChanged(value);
    });

    bindSliderAndSpinBox(m_ui->Slider_HoughPThreshold, m_ui->SpinBox_HoughPThreshold);
    bindSliderAndSpinBox(m_ui->Slider_HoughPMinLineLength, m_ui->SpinBox_HoughPMinLineLength);
    bindSliderAndSpinBox(m_ui->Slider_HoughPMaxLineGap, m_ui->SpinBox_HoughPMaxLineGap);

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
    default:
        m_ui->stackedWidget_LineDetect->setCurrentIndex(0);
    }
}

void LineDetectTabWidget::onLineThresholdChanged(int value)
{
    if (!m_pipelineManager) return;
    {
        PipelineConfig cfg = m_pipelineManager->getConfigSnapshot();
        cfg.lineDetect.threshold = value;
        m_pipelineManager->setConfig(cfg);
    }
    if (m_pipelineManager->getConfigSnapshot().lineDetect.enabled) {
        if (m_onExecutePipeline) {
            m_onExecutePipeline();
        }
    }
}

void LineDetectTabWidget::onLineMinLengthChanged(int value)
{
    if (!m_pipelineManager) return;
    {
        PipelineConfig cfg = m_pipelineManager->getConfigSnapshot();
        cfg.lineDetect.minLength = value;
        m_pipelineManager->setConfig(cfg);
    }
    if (m_pipelineManager->getConfigSnapshot().lineDetect.enabled) {
        if (m_onExecutePipeline) {
            m_onExecutePipeline();
        }
    }
}

void LineDetectTabWidget::onLineMaxGapChanged(int value)
{
    if (!m_pipelineManager) return;
    {
        PipelineConfig cfg = m_pipelineManager->getConfigSnapshot();
        cfg.lineDetect.maxGap = value;
        m_pipelineManager->setConfig(cfg);
    }
    if (m_pipelineManager->getConfigSnapshot().lineDetect.enabled) {
        if (m_onExecutePipeline) {
            m_onExecutePipeline();
        }
    }
}

void LineDetectTabWidget::handleApply()
{
    if (!m_pipelineManager) return;

    LineDetectState state = getLineState();

    // 更新Pipeline配置（使用快照模式，线程安全）
    PipelineConfig cfg = m_pipelineManager->getConfigSnapshot();
    cfg.lineDetect.algorithm = state.algorithm;
    cfg.lineDetect.threshold = state.threshold;
    cfg.lineDetect.minLength = state.minLength;
    cfg.lineDetect.maxGap = state.maxGap;
    
    // 判断是否启用参考线匹配模式
    bool useReferenceLineMatch = cfg.lineDetect.enableReferenceLineMatch && cfg.lineDetect.referenceLineValid;
    
    if (useReferenceLineMatch) {
        // 参考线匹配模式：禁用普通直线检测
        cfg.lineDetect.enabled = false;
        Logger::instance()->info("使用参考线匹配模式检测直线");
    } else {
        // 普通直线检测模式
        cfg.lineDetect.enabled = true;
        // 如果用户勾选了参考线匹配但参考线无效，提示而不重置勾选状态
        if (cfg.lineDetect.enableReferenceLineMatch && !cfg.lineDetect.referenceLineValid) {
            m_ui->label_referenceLineStatus->setText(
                "状态：参考线未绘制，已切换为普通直线检测模式\n请先绘制参考线再启用参考线匹配");
            m_ui->label_referenceLineStatus->setStyleSheet("color: #F59E0B;");
        }
        Logger::instance()->info("使用普通直线检测模式");
    }
    m_pipelineManager->setConfig(cfg);

    // 设置显示模式为LineDetect
    m_pipelineManager->setDisplayMode(DisplayConfig::Mode::LineDetect);

    if (m_onExecutePipeline)
    {
        m_onExecutePipeline();
    }
}

LineDetectTabWidget::LineDetectState LineDetectTabWidget::getLineState() const
{
    LineDetectState state;
    state.algorithm = m_ui->comboBox_lineAlgorithm->currentIndex();
    state.threshold = m_ui->Slider_HoughPThreshold->value();
    state.minLength = m_ui->Slider_HoughPMinLineLength->value();
    state.maxGap = m_ui->Slider_HoughPMaxLineGap->value();
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
    m_ui->Slider_HoughPMinLineLength->setValue(state.minLength);
    m_ui->Slider_HoughPMaxLineGap->setValue(state.maxGap);

    m_ui->SpinBox_HoughPThreshold->setValue(state.threshold);
    m_ui->SpinBox_HoughPMinLineLength->setValue(state.minLength);
    m_ui->SpinBox_HoughPMaxLineGap->setValue(state.maxGap);
}

void LineDetectTabWidget::setLineConfig(const LineDetectConfig& config)
{
    // 设置基础直线检测参数
    LineDetectState state;
    state.algorithm = config.algorithm;
    state.threshold = config.threshold;
    state.minLength = config.minLength;
    state.maxGap = config.maxGap;
    setLineState(state);

    // 同步到PipelineConfig
    if (m_pipelineManager) {
        PipelineConfig cfg = m_pipelineManager->getConfigSnapshot();
        cfg.lineDetect = config;
        m_pipelineManager->setConfig(cfg);
    }
}

// ========== 参考线匹配槽函数 ==========

void LineDetectTabWidget::onReferenceLineEnabledChanged(bool enabled)
{
    if (!m_pipelineManager) return;

    PipelineConfig cfg = m_pipelineManager->getConfigSnapshot();

    if (enabled && !cfg.lineDetect.referenceLineValid) {
        // 启用参考线匹配但未绘制参考线，提示用户
        m_ui->label_referenceLineStatus->setText(
            "状态：请先点击\"绘制参考线\"按钮在图像上绘制参考线");
        m_ui->label_referenceLineStatus->setStyleSheet("color: #F59E0B;");
    } else {
        m_ui->label_referenceLineStatus->setStyleSheet("");
    }

    cfg.lineDetect.enableReferenceLineMatch = enabled;
    if (enabled) {
        cfg.lineDetect.enabled = false;
    }
    m_pipelineManager->setConfig(cfg);

    updateReferenceLineStatus();

    if (m_onExecutePipeline) {
        m_onExecutePipeline();
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
    {
        PipelineConfig cfg = m_pipelineManager->getConfigSnapshot();
        cfg.lineDetect.referenceLineValid = false;
        cfg.lineDetect.referenceLineStart = cv::Point2f(0, 0);
        cfg.lineDetect.referenceLineEnd = cv::Point2f(0, 0);
        m_pipelineManager->setConfig(cfg);
    }

    updateReferenceLineStatus();

    // 发射信号请求清除绘制
    emit requestClearReferenceLine();

    if (m_onExecutePipeline) {
        m_onExecutePipeline();
    }
}

void LineDetectTabWidget::onAngleThresholdChanged(double value)
{
    if (!m_pipelineManager) return;

    PipelineConfig cfg = m_pipelineManager->getConfigSnapshot();
    cfg.lineDetect.angleThreshold = value;
    m_pipelineManager->setConfig(cfg);

    if (cfg.lineDetect.enableReferenceLineMatch && cfg.lineDetect.referenceLineValid) {
        if (m_onExecutePipeline) {
            m_onExecutePipeline();
        }
    }
}

void LineDetectTabWidget::onDistanceThresholdChanged(double value)
{
    if (!m_pipelineManager) return;

    PipelineConfig cfg = m_pipelineManager->getConfigSnapshot();
    cfg.lineDetect.distanceThreshold = value;
    m_pipelineManager->setConfig(cfg);

    if (cfg.lineDetect.enableReferenceLineMatch && cfg.lineDetect.referenceLineValid) {
        if (m_onExecutePipeline) {
            m_onExecutePipeline();
        }
    }
}

void LineDetectTabWidget::onSearchRegionWidthChanged(int value)
{
    if (!m_pipelineManager) return;

    PipelineConfig cfg = m_pipelineManager->getConfigSnapshot();
    cfg.lineDetect.searchRegionWidth = value;
    m_pipelineManager->setConfig(cfg);

    if (cfg.lineDetect.enableReferenceLineMatch && cfg.lineDetect.referenceLineValid) {
        if (m_onExecutePipeline) {
            m_onExecutePipeline();
        }
    }
}

void LineDetectTabWidget::updateReferenceLineStatus()
{
    if (!m_pipelineManager || !m_ui) return;

    PipelineConfig cfg = m_pipelineManager->getConfigSnapshot();
    QString status;

    if (!cfg.lineDetect.enableReferenceLineMatch) {
        status = "状态：参考线匹配未启用";
    } else if (!cfg.lineDetect.referenceLineValid) {
        status = "状态：参考线未绘制\n点击\"绘制参考线\"按钮开始";
    } else {
        status = QString("状态：参考线已绘制\n起点:(%1,%2)\n终点:(%3,%4)")
                     .arg(cfg.lineDetect.referenceLineStart.x, 0, 'f', 1)
                     .arg(cfg.lineDetect.referenceLineStart.y, 0, 'f', 1)
                     .arg(cfg.lineDetect.referenceLineEnd.x, 0, 'f', 1)
                     .arg(cfg.lineDetect.referenceLineEnd.y, 0, 'f', 1);
    }

    m_ui->label_referenceLineStatus->setText(status);
}

void LineDetectTabWidget::connectSignals(const SignalContext& ctx,
                                         std::function<void()> onExecutePipeline,
                                         std::function<void()> onConfigSaved)
{
    Q_UNUSED(onConfigSaved);
    m_onExecutePipeline = onExecutePipeline;

    // [NOTE] 直线检测配置变化时同步到DetectionItem.config
    auto syncToDetectionItem = [this, ctx]() {
        if (ctx.roiCtrl) {
            ctx.roiCtrl->updateLineDetectionConfig(getLineState());
        }
    };

    connect(m_ui->comboBox_lineAlgorithm, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this, syncToDetectionItem](int) { syncToDetectionItem(); });
    connect(m_ui->Slider_HoughPThreshold, &QSlider::valueChanged,
            this, [this, syncToDetectionItem](int) { syncToDetectionItem(); });
    connect(m_ui->Slider_HoughPMinLineLength, &QSlider::valueChanged,
            this, [this, syncToDetectionItem](int) { syncToDetectionItem(); });
    connect(m_ui->Slider_HoughPMaxLineGap, &QSlider::valueChanged,
            this, [this, syncToDetectionItem](int) { syncToDetectionItem(); });

    connect(this, &LineDetectTabWidget::requestDrawReferenceLine,
            ctx.view, &ImageView::startReferenceLineDrawing);
    connect(this, &LineDetectTabWidget::requestClearReferenceLine,
            ctx.view, [view = ctx.view, this]() {
                view->clearReferenceLine();
                if (m_onExecutePipeline) m_onExecutePipeline();
            });
    connect(ctx.view, &ImageView::referenceLineDrawn,
            this, &LineDetectTabWidget::setReferenceLine);
}

void LineDetectTabWidget::setReferenceLine(const cv::Point2f& start, const cv::Point2f& end)
{
    if (!m_pipelineManager) return;

    {
        PipelineConfig cfg = m_pipelineManager->getConfigSnapshot();
        cfg.lineDetect.referenceLineStart = start;
        cfg.lineDetect.referenceLineEnd = end;
        cfg.lineDetect.referenceLineValid = true;
        m_pipelineManager->setConfig(cfg);
    }

    updateReferenceLineStatus();

    // 自动启用参考线匹配
    m_ui->chk_enableReferenceLine->setChecked(true);

    if (m_onExecutePipeline) {
        m_onExecutePipeline();
    }
}

void LineDetectTabWidget::updateMatchResultStatus(int matchedCount, int totalCount, double angleThreshold, double distanceThreshold, bool isReferenceMode)
{
    if (!m_ui) return;

    QString status;

    if (isReferenceMode) {
        if (matchedCount > 0) {
            status = QString("匹配结果: %1/%2 条直线匹配成功\n角度容差: %3°, 距离容差: %4px")
                         .arg(matchedCount)
                         .arg(totalCount)
                         .arg(angleThreshold)
                         .arg(distanceThreshold);
        } else if (totalCount > 0) {
            status = QString("匹配结果: 未找到匹配直线 (检测到 %1 条)\n角度容差: %2°, 距离容差: %3px")
                         .arg(totalCount)
                         .arg(angleThreshold)
                         .arg(distanceThreshold);
        } else {
            status = QString("匹配结果: 未找到匹配直线\n角度容差: %1°, 距离容差: %2px")
                         .arg(angleThreshold)
                         .arg(distanceThreshold);
        }
    } else {
        if (totalCount > 0) {
            status = QString("直线检测完成: 共检测到 %1 条直线").arg(totalCount);
        } else {
            status = QString("未检测到直线");
        }
    }

    m_ui->label_referenceLineStatus->setText(status);
}
