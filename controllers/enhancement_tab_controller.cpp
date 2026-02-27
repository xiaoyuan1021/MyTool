#include "enhancement_tab_controller.h"
#include "logger.h"
EnhancementTabController::EnhancementTabController(Ui::MainWindow *ui, PipelineManager *pipeline, QTimer *debounceTimer, std::function<void()> processCallback, QObject *parent):
QObject(parent),
    m_ui(ui),
    m_pipeline(pipeline),
    m_debounceTimer(debounceTimer),
    m_processCallback(std::move(processCallback))
{
}

void EnhancementTabController::initialize()
{
    connect(m_ui->Slider_brightness, &QSlider::valueChanged, this, &EnhancementTabController::handleBrightnessChanged);
    connect(m_ui->Slider_contrast, &QSlider::valueChanged, this, &EnhancementTabController::handleContrastChanged);
    connect(m_ui->Slider_gamma, &QSlider::valueChanged, this, &EnhancementTabController::handleGammaChanged);
    connect(m_ui->Slider_sharpen, &QSlider::valueChanged, this, &EnhancementTabController::handleSharpenChanged);
    connect(m_ui->btn_resetBC, &QPushButton::clicked, this, &EnhancementTabController::handleReset);
    connect(m_ui->btn_saveBC, &QPushButton::clicked, this, &EnhancementTabController::handleSave);
    connect(m_ui->btn_undoBC, &QPushButton::clicked, this, &EnhancementTabController::handleUndo);

    m_enhancementHistory.push(captureState());
    updateUndoUi();
}

void EnhancementTabController::handleBrightnessChanged(int value)
{
    Q_UNUSED(value);
    if (!m_pipeline || !m_debounceTimer) return;
    m_pipeline->setGrayFilterEnabled(false);
    m_debounceTimer->start();
}

void EnhancementTabController::handleContrastChanged(int value)
{
    Q_UNUSED(value);
    if (!m_pipeline || !m_debounceTimer) return;
    m_pipeline->setGrayFilterEnabled(false);
    m_debounceTimer->start();
}

void EnhancementTabController::handleGammaChanged(int value)
{
    Q_UNUSED(value);
    if (!m_pipeline || !m_debounceTimer) return;
    m_pipeline->setGrayFilterEnabled(false);
    m_debounceTimer->start();
}

void EnhancementTabController::handleSharpenChanged(int value)
{
    Q_UNUSED(value);
    if (!m_pipeline || !m_debounceTimer) return;
    m_pipeline->setGrayFilterEnabled(false);
    m_debounceTimer->start();
}

EnhancementTabController::EnhancementState EnhancementTabController::captureState() const
{
    EnhancementState state;
    state.brightness = m_ui->Slider_brightness->value();
    state.contrast   = m_ui->Slider_contrast->value();
    state.gamma      = m_ui->Slider_gamma->value();
    state.sharpen    = m_ui->Slider_sharpen->value();
    return state;
}

void EnhancementTabController::applyState(const EnhancementState &state)
{
    const QSignalBlocker b1(m_ui->Slider_brightness);
    const QSignalBlocker b2(m_ui->Slider_contrast);
    const QSignalBlocker b3(m_ui->Slider_gamma);
    const QSignalBlocker b4(m_ui->Slider_sharpen);

    m_ui->Slider_brightness->setValue(state.brightness);
    m_ui->Slider_contrast->setValue(state.contrast);
    m_ui->Slider_gamma->setValue(state.gamma);
    m_ui->Slider_sharpen->setValue(state.sharpen);

    if (m_processCallback) m_processCallback();
}

void EnhancementTabController::pushSnapshot(const EnhancementState &state)
{
    if (m_enhancementHistory.isEmpty() || !(state == m_enhancementHistory.top())) {
        m_enhancementHistory.push(state);
        updateUndoUi();
    }
}

void EnhancementTabController::updateUndoUi()
{
    bool hasSnapshot = !m_enhancementHistory.isEmpty();
    bool canStepBack = m_enhancementHistory.size() > 1;
    m_ui->btn_undoBC->setEnabled(hasSnapshot && canStepBack);
}

void EnhancementTabController::handleReset()
{
    if (!m_pipeline) return;
    m_ui->Slider_brightness->setValue(0);
    m_ui->Slider_contrast->setValue(100);
    m_ui->Slider_gamma->setValue(100);
    m_ui->Slider_sharpen->setValue(100);
    m_pipeline->resetEnhancement();
    m_pipeline->setGrayFilterEnabled(false);

    EnhancementState snapshot = captureState();
    m_enhancementHistory.clear();
    m_enhancementHistory.push(snapshot);
    updateUndoUi();

    Logger::instance()->info("增强参数已重置");
    if (m_processCallback) m_processCallback();
}

void EnhancementTabController::handleSave()
{
    EnhancementState current = captureState();
    pushSnapshot(current);
    Logger::instance()->info("增强参数已保存为快照");
}

void EnhancementTabController::handleUndo()
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
    //updateEnhancementUndoState();
}
