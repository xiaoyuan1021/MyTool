#pragma once

#include <QObject>
#include "ui_mainwindow.h"
#include "pipeline_manager.h"
#include <QListWidgetItem>

namespace Ui {
    class Form_Process;
}

class ProcessTabWidget : public QWidget
{
    Q_OBJECT

public:
    explicit ProcessTabWidget(PipelineManager* pipelineManager, QWidget *parent = nullptr);
    ~ProcessTabWidget();
    void initialize();
    void refreshAlgorithmListUI(const QVector<AlgorithmStep>& algorithmQueue);

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
    Ui::Form_Process *m_ui;
    PipelineManager* m_pipelineManager;
    int m_editingAlgorithmIndex = -1;
};
