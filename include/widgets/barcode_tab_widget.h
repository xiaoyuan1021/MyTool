#pragma once

#include <QWidget>
#include <functional>
#include "pipeline_manager.h"
#include "widgets/i_signal_connectable.h"
#include "widgets/i_configurable_tab.h"
#include "widgets/i_tab_initializable.h"

namespace Ui
{
class BarcodeTabForm;
}

class BarcodeTabWidget : public QWidget, public ISignalConnectable, public IConfigurableTab, public ITabInitializable
{
    Q_OBJECT

public:
    explicit BarcodeTabWidget(PipelineManager* pipelineManager,
                              std::function<void()> processCallback,
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
    void saveToConfig(AppConfig& config) const override;
    void loadFromConfig(const AppConfig& config) override;

    // ITabInitializable 接口实现
    void initializeTab(const TabInitContext& ctx) override;

    void connectSignals(PipelineManager* pm, RoiManager* rm,
                        ImageView* view, RoiUiController* roiCtrl,
                        std::function<void()> requestRefresh,
                        std::function<void()> processAndDisplay) override;

private:
    PipelineManager* m_pipelineManager;
    std::function<void()> m_processCallback;
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
    // 请求应用条码识别设置
    void requestApplyBarcodeSettings();
};