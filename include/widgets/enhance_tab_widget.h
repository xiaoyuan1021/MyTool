# pragma once

#include <QWidget>
#include "core/i_pipeline_access.h"
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
    explicit EnhanceTabWidget(IPipelineAccess* pipelineAccess, QWidget* parent = nullptr);
    ~EnhanceTabWidget();

    // 配置管理
    void getEnhanceConfig(int& brightness, int& contrast, int& gamma, int& sharpen) const;
    void setEnhanceConfig(int brightness, int contrast, int gamma, int sharpen);

    // IConfigurableTab 接口实现
    void saveToConfig(PipelineConfig& config) const override;
    void loadFromConfig(const PipelineConfig& config) override;

    void connectSignals(const SignalContext& ctx,
                        std::function<void()> onExecutePipeline,
                        std::function<void()> onConfigSaved = nullptr) override;

signals:
    void processRequested();

private slots:
    void onSliderChanged();
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
    /// 轻量预览：仅对基图做增强处理并更新画布，不执行完整 Pipeline
    void previewEnhance();
    
    QStack<EnhancementState> m_enhancementHistory;
    Ui::EnhanceTabWidget* m_ui;
    IPipelineAccess* m_pipeline;
    ImageView* m_view = nullptr;
    RoiManager* m_roiManager = nullptr;
};
