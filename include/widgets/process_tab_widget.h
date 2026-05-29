#pragma once

#include <QObject>
#include "pipeline_manager.h"
#include <QListWidgetItem>
#include "widgets/i_tab_interfaces.h"

namespace Ui {
    class Form_Process;
}

class ProcessTabWidget : public QWidget, public ISignalConnectable
{
    Q_OBJECT

public:
    explicit ProcessTabWidget(PipelineManager* pipelineManager, QWidget *parent = nullptr);
    ~ProcessTabWidget();
    void initialize();
    void refreshAlgorithmListUI(const QVector<AlgorithmStep>& algorithmQueue);

    void connectSignals(const SignalContext& ctx,
                        std::function<void()> onExecutePipeline,
                        std::function<void()> onConfigSaved = nullptr) override;

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
    bool m_loadingParameters = false;
};
