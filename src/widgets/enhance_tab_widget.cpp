#include "enhance_tab_widget.h"
#include "ui_enhance_tab.h"
#include "config/constants.h"
#include "config/config_manager.h"
#include "ui/slider_spinbox_binder.h"
#include "controllers/roi_ui_controller.h"
#include "image_view.h"
#include "algorithm/image_utils.h"
#include <opencv2/imgproc.hpp>

EnhanceTabWidget::EnhanceTabWidget(IPipelineAccess* pipelineAccess, QWidget* parent)
    : QWidget(parent)
    , m_ui(new Ui::EnhanceTabWidget)
    , m_pipeline(pipelineAccess)
{
    m_ui->setupUi(this);

    // 设置 slider 和 spinbox 的范围、步长和默认值
    m_ui->Slider_brightness->setRange(-100, 100);
    m_ui->Slider_brightness->setValue(0);
    m_ui->spinBox_brightness->setRange(-100, 100);
    m_ui->spinBox_brightness->setValue(0);
    bindSliderAndSpinBox(m_ui->Slider_brightness, m_ui->spinBox_brightness);

    m_ui->Slider_contrast->setRange(0, 300);
    m_ui->Slider_contrast->setValue(100);
    m_ui->spinBox_contrast->setRange(0, 300);
    m_ui->spinBox_contrast->setValue(100);
    bindSliderAndSpinBox(m_ui->Slider_contrast, m_ui->spinBox_contrast);

    m_ui->Slider_gamma->setRange(10, 300);
    m_ui->Slider_gamma->setValue(100);
    m_ui->spinBox_gamma->setRange(10, 300);
    m_ui->spinBox_gamma->setValue(100);
    bindSliderAndSpinBox(m_ui->Slider_gamma, m_ui->spinBox_gamma);

    m_ui->Slider_sharpen->setRange(0, 500);
    m_ui->Slider_sharpen->setValue(100);
    m_ui->spinBox_sharpen->setRange(0, 500);
    m_ui->spinBox_sharpen->setValue(100);
    bindSliderAndSpinBox(m_ui->Slider_sharpen, m_ui->spinBox_sharpen);

    // 所有 slider 变化统一走 onSliderChanged
    connect(m_ui->Slider_brightness, &QSlider::valueChanged, this, &EnhanceTabWidget::onSliderChanged);
    connect(m_ui->Slider_contrast, &QSlider::valueChanged, this, &EnhanceTabWidget::onSliderChanged);
    connect(m_ui->Slider_gamma, &QSlider::valueChanged, this, &EnhanceTabWidget::onSliderChanged);
    connect(m_ui->Slider_sharpen, &QSlider::valueChanged, this, &EnhanceTabWidget::onSliderChanged);

    m_enhancementHistory.push(captureState());
    updateUndoUi();
}

void EnhanceTabWidget::onSliderChanged()
{
    syncConfigToPipeline();
    previewEnhance();
}

void EnhanceTabWidget::on_btn_resetBC_clicked()
{
    if (!m_pipeline) return;
    m_ui->Slider_brightness->setValue(0);
    m_ui->Slider_contrast->setValue(100);
    m_ui->Slider_gamma->setValue(100);
    m_ui->Slider_sharpen->setValue(100);
    m_pipeline->resetConfigToDefaults();

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
    syncConfigToPipeline();
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

    // 同步参数到 Pipeline（线程安全：使用快照+setConfig模式）
    {
        PipelineConfig cfg = m_pipeline->getConfigSnapshot();
        cfg.enhance.brightness = state.brightness;
        cfg.enhance.contrast = state.contrast;
        cfg.enhance.gamma = state.gamma;
        cfg.enhance.sharpen = state.sharpen;
        m_pipeline->setConfig(cfg);
    }

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
    PipelineConfig cfg = m_pipeline->getConfigSnapshot();
    cfg.enhance.brightness = current.brightness;
    cfg.enhance.contrast = current.contrast;
    cfg.enhance.gamma = current.gamma;
    cfg.enhance.sharpen = current.sharpen;
    m_pipeline->setConfig(cfg);
}

void EnhanceTabWidget::previewEnhance()
{
    if (!m_view || !m_roiManager) return;

    // 获取当前 ROI 图像（与 Pipeline 使用相同的基图）
    cv::Mat base = m_roiManager->getCurrentImage();
    if (base.empty()) return;

    // 获取当前增强参数
    int brightness = m_ui->Slider_brightness->value();
    int contrast   = m_ui->Slider_contrast->value();
    int gamma      = m_ui->Slider_gamma->value();
    int sharpen    = m_ui->Slider_sharpen->value();

    // 轻量增强处理：亮度/对比度 + Gamma + 锐化（与 Pipeline adjustParameter 保持一致）
    cv::Mat result = base.clone();

    // 亮度 + 对比度
    result.convertTo(result, -1, contrast / 100.0, brightness);

    // Gamma 校正（与 Pipeline adjustParameter 保持一致）
    double g = gamma / 100.0;
    cv::Mat lookUpLUT(1, 256, CV_8U);
    uchar* p = lookUpLUT.ptr();
    for (int i = 0; i < 256; ++i) {
        p[i] = cv::saturate_cast<uchar>(std::pow(i / 255.0, g) * 255.0);
    }
    cv::LUT(result, lookUpLUT, result);

    // 锐化（USM，与 ImageProcessor::adjustParameter 一致）
    double sharpenVal = sharpen / 100.0;
    if (sharpenVal > 0.0) {
        cv::Mat blurred;
        cv::GaussianBlur(result, blurred, cv::Size(0, 0), 1.0);
        cv::addWeighted(result, 1.0 + sharpenVal, blurred, -sharpenVal, 0, result);
    }

    m_view->setImage(ImageUtils::matToQImage(result));
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
    applyStateQuiet(state);
}

void EnhanceTabWidget::applyStateQuiet(const EnhancementState &state)
{
    // 阻断所有slider和spinbox信号，防止级联触发processRequested
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

    // 仅同步参数到Pipeline，不触发处理
    PipelineConfig cfg = m_pipeline->getConfigSnapshot();
    cfg.enhance.brightness = state.brightness;
    cfg.enhance.contrast = state.contrast;
    cfg.enhance.gamma = state.gamma;
    cfg.enhance.sharpen = state.sharpen;
    m_pipeline->setConfig(cfg);
}

void EnhanceTabWidget::connectSignals(const SignalContext& ctx,
                                      std::function<void()> onExecutePipeline,
                                      std::function<void()> onConfigSaved)
{
    Q_UNUSED(onConfigSaved);
    m_view = ctx.view;
    m_roiManager = ctx.roiManager;
    connect(this, &EnhanceTabWidget::processRequested,
            this, [onExecutePipeline]() { onExecutePipeline(); });
}

// ========== IConfigurableTab 接口实现 ==========

void EnhanceTabWidget::saveToConfig(PipelineConfig& config) const
{
    config.enhance.brightness = m_ui->Slider_brightness->value();
    config.enhance.contrast   = m_ui->Slider_contrast->value();
    config.enhance.gamma      = m_ui->Slider_gamma->value();
    config.enhance.sharpen    = m_ui->Slider_sharpen->value();
}

void EnhanceTabWidget::loadFromConfig(const PipelineConfig& config)
{
    setEnhanceConfig(
        config.enhance.brightness,
        config.enhance.contrast,
        config.enhance.gamma,
        config.enhance.sharpen
    );
}
