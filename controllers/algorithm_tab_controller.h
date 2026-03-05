#pragma once

#include <QObject>
#include "ui_mainwindow.h"
#include "pipeline_manager.h"
#include <QListWidgetItem>

class AlgorithmTabController : public QObject
{
    Q_OBJECT
public:
    AlgorithmTabController(Ui::MainWindow* ui, PipelineManager* pipeline, QObject* parent = nullptr);
    void initialize();

signals:
    void algorithmChanged();

private slots:
    void addAlgorithm();
    void removeAlgorithm();
    void moveAlgorithmUp();
    void moveAlgorithmDown();
    void onAlgorithmTypeChanged(int index);
    void onAlgorithmSelectionChanged(QListWidgetItem* current, QListWidgetItem* previous);

private:
    void setupConnections();
    void saveCurrentEdit();
    void loadAlgorithmParameters(int index);

private:
    Ui::MainWindow* m_ui;
    PipelineManager* m_pipeline;
    int m_editingAlgorithmIndex = -1;
};