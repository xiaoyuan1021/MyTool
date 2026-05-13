#pragma once

#include <QWidget>
#include "pipeline_manager.h"
#include "widgets/i_tab_interfaces.h"

namespace Ui {
    class Form_Judge;
}

class JudgeTabWidget : public QWidget, public ISignalConnectable, public IConfigurableTab, public IResultUpdatable
{
    Q_OBJECT

public:
    explicit JudgeTabWidget(PipelineManager* pipelineManager, QWidget *parent = nullptr);
    ~JudgeTabWidget();
    void setCurrentRegionCount(int count);

    // IResultUpdatable 接口实现
    void updateFromPipeline(const PipelineContext& ctx) override;

    void getJudgeConfig(int& minCount, int& maxCount, int& currentCount) const;
    void setJudgeConfig(int minCount, int maxCount, int currentCount);

    // IConfigurableTab 接口实现
    void saveToConfig(PipelineConfig& config) const override;
    void loadFromConfig(const PipelineConfig& config) override;

    void connectSignals(PipelineManager* pm, RoiManager* rm,
                        ImageView* view, RoiUiController* roiCtrl,
                        std::function<void()> requestRefresh,
                        std::function<void()> processAndDisplay) override;

signals:
    /// 判定参数变化信号（min/max值改变时触发，用于同步到DetectionItem.config）
    void judgeConfigChanged(int minCount, int maxCount);

private slots:
    void onRunTestClicked();
    void onJudgeValueChanged();

private:
    Ui::Form_Judge *m_ui;
    PipelineManager* m_pipelineManager;
};

