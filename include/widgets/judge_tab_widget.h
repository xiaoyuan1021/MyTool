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

private slots:
    void onRunTestClicked();

private:
    Ui::Form_Judge *m_ui;
    PipelineManager* m_pipelineManager;
};

