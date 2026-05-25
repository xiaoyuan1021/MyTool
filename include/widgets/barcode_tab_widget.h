#pragma once

#include <QWidget>
#include <functional>
#include "pipeline_manager.h"
#include "widgets/i_tab_interfaces.h"

namespace Ui
{
class BarcodeTabForm;
}

class BarcodeTabWidget : public QWidget, public ISignalConnectable, public IConfigurableTab, public ITabInitializable, public IResultUpdatable
{
    Q_OBJECT

public:
    explicit BarcodeTabWidget(PipelineManager* pipelineManager,
                              QWidget* parent = nullptr);
    ~BarcodeTabWidget();
    
    // 设置条码配置（从外部恢复状态）
    void setBarcodeConfig(const BarcodeConfig& config);

    // 更新识别结果表格
    void updateResultsTable(const QVector<BarcodeResult>& results);
    
    // 更新状态显示
    void updateStatus(const QString& status);

    // 从UI获取配置
    BarcodeConfig getBarcodeConfig() const;

    // IConfigurableTab 接口实现
    void saveToConfig(PipelineConfig& config) const override;
    void loadFromConfig(const PipelineConfig& config) override;

    // ITabInitializable 接口实现
    void initializeTab(const TabInitContext& ctx) override;

    // IResultUpdatable 接口实现
    void updateFromPipeline(const PipelineContext& ctx) override;

    void connectSignals(PipelineManager* pm, RoiManager* rm,
                        ImageView* view, RoiUiController* roiCtrl,
                        std::function<void()> onConfigChanged,
                        std::function<void()> onExecuteRequested) override;

private:
    PipelineManager* m_pipelineManager;
    Ui::BarcodeTabForm* m_ui;
    
    // 更新Pipeline配置
    void updatePipelineConfig();

private slots:
    void onEnableBarcodeChanged(bool enabled);
    void onCodeTypeChanged(int index);
    void onMaxNumChanged(int value);
    void onReturnQualityChanged(bool checked);
    void handleApply();

signals:
    // 条码配置发生变化
    void barcodeConfigChanged();
    // 请求应用条码识别设置
    void requestApplyBarcodeSettings();
};