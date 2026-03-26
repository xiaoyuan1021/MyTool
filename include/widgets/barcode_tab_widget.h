#pragma once

#include <QWidget>
#include <functional>
#include "pipeline_manager.h"

namespace Ui
{
class BarcodeTabForm;
}

class BarcodeTabWidget : public QWidget
{
    Q_OBJECT

public:
    explicit BarcodeTabWidget(PipelineManager* pipelineManager,
                              std::function<void()> processCallback,
                              QWidget* parent = nullptr);
    ~BarcodeTabWidget();
    
    // 更新识别结果表格
    void updateResultsTable(const QVector<BarcodeResult>& results);
    
    // 更新状态显示
    void updateStatus(const QString& status);

private:
    PipelineManager* m_pipelineManager;
    std::function<void()> m_processCallback;
    Ui::BarcodeTabForm* m_ui;
    
    // 从UI获取配置
    BarcodeConfig getBarcodeConfig() const;
    
    // 更新Pipeline配置
    void updatePipelineConfig();

private slots:
    void onEnableBarcodeChanged(bool enabled);
    void onCodeTypeChanged(int index);
    void onMaxNumChanged(int value);
    void onReturnQualityChanged(bool checked);
    void onQuietZoneChanged(bool checked);
    void handleApply();

signals:
    // 请求应用条码识别设置
    void requestApplyBarcodeSettings();
};