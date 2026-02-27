#pragma once

#include <QObject>
#include "ui_mainwindow.h"
#include "pipeline_manager.h"

class ImageTabController : public QObject
{
    Q_OBJECT
public:
    ImageTabController(Ui::MainWindow* ui,
                       PipelineManager* pipeline,
                       QObject* parent = nullptr);

    void initialize();
signals:
    void channelChanged(PipelineConfig::Channel channel);

private slots:
    void handleApplyChannel();

private:
    PipelineConfig::Channel channelFromIndex(int index) const;

    Ui::MainWindow* m_ui;
    PipelineManager* m_pipeline;
    bool m_channelFlag = false;
};
