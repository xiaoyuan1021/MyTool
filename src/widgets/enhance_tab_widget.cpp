#include "enhance_tab_widget.h"
#include "ui_enhance_tab.h"

EnhanceTabWidget::EnhanceTabWidget(PipelineManager* pipelineManager, QWidget* parent)
    : QWidget(parent)
    , m_ui(new Ui::EnhanceTabWidget)
    , m_pipelineManager(pipelineManager)
    , m_debounceTimer(new QTimer(this))
{
    m_ui->setupUi(this);

    // 设置 slider 和 spinbox 的范围、步长和默认值
    m_ui->Slider_brightness->setRange(-100, 100);
    m_ui->Slider_brightness->setValue(0);
    m_ui->spinBox_brightness->setRange(-100, 100);
    m_ui->spinBox_brightness->setValue(0);
    connect(m_ui->Slider_brightness, &QSlider::valueChanged, m_ui->spinBox_brightness, &QSpinBox::setValue);
    connect(m_ui->spinBox_brightness, QOverload<int>::of(&QSpinBox::valueChanged), m_ui->Slider_brightness, &QSlider::setValue);

    m_ui->Slider_contrast->setRange(0, 300);
    m_ui->Slider_contrast->setValue(100);
    m_ui->spinBox_contrast->setRange(0, 300);
    m_ui->spinBox_contrast->setValue(100);
    connect(m_ui->Slider_contrast, &QSlider::valueChanged, m_ui->spinBox_contrast, &QSpinBox::setValue);
    connect(m_ui->spinBox_contrast, QOverload<int>::of(&QSpinBox::valueChanged), m_ui->Slider_contrast, &QSlider::setValue);

    m_ui->Slider_gamma->setRange(10, 300);
    m_ui->Slider_gamma->setValue(100);
    m_ui->spinBox_gamma->setRange(10, 300);
    m_ui->spinBox_gamma->setValue(100);
    connect(m_ui->Slider_gamma, &QSlider::valueChanged, m_ui->spinBox_gamma, &QSpinBox::setValue);
    connect(m_ui->spinBox_gamma, QOverload<int>::of(&QSpinBox::valueChanged), m_ui->Slider_gamma, &QSlider::setValue);

    m_ui->Slider_sharpen->setRange(0, 500);
    m_ui->Slider_sharpen->setValue(100);
    m_ui->spinBox_sharpen->setRange(0, 500);
    m_ui->spinBox_sharpen->setValue(100);
    connect(m_ui->Slider_sharpen, &QSlider::valueChanged, m_ui->spinBox_sharpen, &QSpinBox::setValue);
    connect(m_ui->spinBox_sharpen, QOverload<int>::of(&QSpinBox::valueChanged), m_ui->Slider_sharpen, &QSlider::setValue);

    m_enhancementHistory.push(captureState());
    updateUndoUi();
    m_debounceTimer->setInterval(200);
    connect(m_debounceTimer, &QTimer::timeout, this, [this]()
    {
        emit processRequested();
        m_debounceTimer->stop();
    });
}

void EnhanceTabWidget::on_Slider_brightness_valueChanged(int value)
{
    Q_UNUSED(value);
    syncConfigToPipeline();
    m_debounceTimer->start();
}

void EnhanceTabWidget::on_Slider_contrast_valueChanged(int value)
{
    Q_UNUSED(value);
    syncConfigToPipeline();
    m_debounceTimer->start();
}

void EnhanceTabWidget::on_Slider_gamma_valueChanged(int value)
{
    Q_UNUSED(value);
    syncConfigToPipeline();
    m_debounceTimer->start();
}

void EnhanceTabWidget::on_Slider_sharpen_valueChanged(int value)
{
    Q_UNUSED(value);
    syncConfigToPipeline();
    m_debounceTimer->start();
}

void EnhanceTabWidget::on_btn_resetBC_clicked()
{
    if (!m_pipelineManager) return;
    m_ui->Slider_brightness->setValue(0);
    m_ui->Slider_contrast->setValue(100);
    m_ui->Slider_gamma->setValue(100);
    m_ui->Slider_sharpen->setValue(100);
    m_pipelineManager->resetEnhancement();

    EnhancementState snapshot = captureState();
    m_enhancementHistory.clear();
    m_enhancementHistory.push(snapshot);
    updateUndoUi();
    applyState(snapshot);
}

void EnhanceTabWidget::on_btn_saveBC_clicked()
{
    EnhancementState current = captureState();
    pushSnapshot(current);
}

void EnhanceTabWidget::on_btn_undoBC_clicked()
{
    if (m_enhancementHistory.isEmpty())
        return;

    EnhancementState current = captureState();
    const EnhancementState& latest = m_enhancementHistory.top();

    // 先撤销未保存的改动
    if (!(current == latest)) {
        applyState(latest);
        return;
    }

    // 继续回溯到更早快照
    if (m_enhancementHistory.size() > 1) {
        m_enhancementHistory.pop();
        applyState(m_enhancementHistory.top());
    }
    updateUndoUi();
}

EnhanceTabWidget::EnhancementState EnhanceTabWidget::captureState() const
{
    EnhancementState state;
    state.brightness = m_ui->Slider_brightness->value();
    state.contrast   = m_ui->Slider_contrast->value();
    state.gamma      = m_ui->Slider_gamma->value();
    state.sharpen    = m_ui->Slider_sharpen->value();
    return state;
}

void EnhanceTabWidget::applyState(const EnhancementState &state)
{
    const QSignalBlocker b1(m_ui->Slider_brightness);
    const QSignalBlocker b2(m_ui->Slider_contrast);
    const QSignalBlocker b3(m_ui->Slider_gamma);
    const QSignalBlocker b4(m_ui->Slider_sharpen);
    const QSignalBlocker sb1(m_ui->spinBox_brightness);
    const QSignalBlocker sb2(m_ui->spinBox_contrast);
    const QSignalBlocker sb3(m_ui->spinBox_gamma);
    const QSignalBlocker sb4(m_ui->spinBox_sharpen);

    m_ui->Slider_brightness->setValue(state.brightness);
    m_ui->Slider_contrast->setValue(state.contrast);
    m_ui->Slider_gamma->setValue(state.gamma);
    m_ui->Slider_sharpen->setValue(state.sharpen);

    m_ui->spinBox_brightness->setValue(state.brightness);
    m_ui->spinBox_contrast->setValue(state.contrast);
    m_ui->spinBox_gamma->setValue(state.gamma);
    m_ui->spinBox_sharpen->setValue(state.sharpen);

    // 同步参数到 Pipeline
    m_pipelineManager->getConfig().brightness = state.brightness;
    m_pipelineManager->getConfig().contrast = state.contrast / 100.0;
    m_pipelineManager->getConfig().gamma = state.gamma / 100.0;
    m_pipelineManager->getConfig().sharpen = state.sharpen / 100.0;

    emit processRequested();
}

void EnhanceTabWidget::pushSnapshot(const EnhancementState &state)
{
    if (m_enhancementHistory.isEmpty() || !(state == m_enhancementHistory.top())) 
    {
        m_enhancementHistory.push(state);
        updateUndoUi();
    }
}

void EnhanceTabWidget::updateUndoUi()
{
    bool hasSnapshot = !m_enhancementHistory.isEmpty();
    bool canStepBack = m_enhancementHistory.size() > 1;
    m_ui->btn_undoBC->setEnabled(hasSnapshot && canStepBack);
}

void EnhanceTabWidget::syncConfigToPipeline()
{
    EnhancementState current = captureState();
    m_pipelineManager->getConfig().brightness = current.brightness;
    m_pipelineManager->getConfig().contrast = current.contrast / 100.0;
    m_pipelineManager->getConfig().gamma = current.gamma / 100.0;
    m_pipelineManager->getConfig().sharpen = current.sharpen / 100.0;
    
    // 通知外部将配置回写到当前ROI，防止ROI之间配置串扰
    emit enhanceConfigChanged();
}

EnhanceTabWidget::~EnhanceTabWidget()
{
    delete m_ui;
}

void EnhanceTabWidget::getEnhanceConfig(int& brightness, int& contrast, int& gamma, int& sharpen) const
{
    EnhancementState state = captureState();
    brightness = state.brightness;
    contrast = state.contrast;
    gamma = state.gamma;
    sharpen = state.sharpen;
}

void EnhanceTabWidget::setEnhanceConfig(int brightness, int contrast, int gamma, int sharpen)
{
    EnhancementState state;
    state.brightness = brightness;
    state.contrast = contrast;
    state.gamma = gamma;
    state.sharpen = sharpen;
    applyState(state);
}