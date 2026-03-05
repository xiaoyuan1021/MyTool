#pragma once

#include <QObject>
#include "ui_mainwindow.h"
#include "pipeline_manager.h"
#include <QTimer>

class FilterTabController : public QObject
{
    Q_OBJECT
public:
    FilterTabController(Ui::MainWindow* ui,
                       PipelineManager* pipeline,
                       QObject* parent = nullptr);
    void initialize();
    void configureColorFilter(int filterModeIndex);

signals:
    void filterConfigChanged();

private:
    Ui::MainWindow* m_ui;
    PipelineManager* m_pipeline;
    QTimer* m_debounceTimer = nullptr;

    void setupConnections();
    void syncRGBParameters();
    void syncHSVParameters();
    
private slots:
    void handleFilterModeChanged(int index);
    void filterColorChannelsChanged();
};