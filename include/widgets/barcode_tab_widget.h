#pragma once

#include <QWidget>
#include <functional>
#include "core/i_pipeline_access.h"
#include "widgets/i_tab_interfaces.h"

namespace Ui
{
class BarcodeTabForm;
}

class BarcodeTabWidget : public QWidget, public ISignalConnectable, public IConfigurableTab, public ITabInitializable, public IResultUpdatable
{
    Q_OBJECT

public:
    explicit BarcodeTabWidget(IPipelineAccess* pipelineAccess,
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

    void connectSignals(const SignalContext& ctx,
                        std::function<void()> onExecutePipeline,
                        std::function<void()> onConfigSaved = nullptr) override;

private:
    IPipelineAccess* m_pipeline;
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
