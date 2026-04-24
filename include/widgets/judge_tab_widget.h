#pragma once

#include <QWidget>
#include "pipeline_manager.h"

namespace Ui {
    class Form_Judge;
}

class JudgeTabWidget : public QWidget
{
    Q_OBJECT

public:
    explicit JudgeTabWidget(PipelineManager* pipelineManager, QWidget *parent = nullptr);
    ~JudgeTabWidget();
    void setCurrentRegionCount(int count);

    void getJudgeConfig(int& minCount, int& maxCount, int& currentCount) const;
    void setJudgeConfig(int minCount, int maxCount, int currentCount);

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

