# pragma once

#include <QWidget>
#include "pipeline_manager.h"
#include "pipeline.h"
#include "widgets/i_tab_interfaces.h"
#include <QObject>
#include <QStack>
#include <QSignalBlocker>
#include <functional>
#include "ui_enhance_tab.h"


namespace Ui {
class EnhanceTabWidget;
}

class EnhanceTabWidget : public QWidget, public ISignalConnectable, public IConfigurableTab {
    Q_OBJECT

public:
    explicit EnhanceTabWidget(PipelineManager* pipelineManager, QWidget* parent = nullptr);
    ~EnhanceTabWidget();

    // 配置管理
    void getEnhanceConfig(int& brightness, int& contrast, int& gamma, int& sharpen) const;
    void setEnhanceConfig(int brightness, int contrast, int gamma, int sharpen);

    // IConfigurableTab 接口实现
    void saveToConfig(PipelineConfig& config) const override;
    void loadFromConfig(const PipelineConfig& config) override;

    void connectSignals(PipelineManager* pm, RoiManager* rm,
                        ImageView* view, RoiUiController* roiCtrl,
                        std::function<void()> requestRefresh,
                        std::function<void()> processAndDisplay) override;

signals:
    void brightnessChanged(int);
    void contrastChanged(int);
    void gammaChanged(int);
    void sharpenChanged(int);
    void processRequested();
    /** 增强参数变化时触发，用于通知外部将配置回写到当前ROI */
    void enhanceConfigChanged();

private slots:

    void on_Slider_brightness_valueChanged(int value);
    void on_Slider_contrast_valueChanged(int value);
    void on_Slider_gamma_valueChanged(int value);
    void on_Slider_sharpen_valueChanged(int value);
    void on_btn_resetBC_clicked();
    void on_btn_saveBC_clicked();
    void on_btn_undoBC_clicked();

private:
    // 复用 EnhanceConfig 类型定义
    using EnhancementState = EnhanceConfig;
    EnhancementState captureState() const;
    /// 应用状态并触发处理（用户交互时使用）
    void applyState(const EnhancementState& state);
    /// 仅设置UI值，不触发处理（外部同步配置时使用）
    void applyStateQuiet(const EnhancementState& state);
    void pushSnapshot(const EnhancementState& state);
    void updateUndoUi();
    void syncConfigToPipeline();
    
    QStack<EnhancementState> m_enhancementHistory;
    Ui::EnhanceTabWidget* m_ui;
    PipelineManager* m_pipelineManager;
};