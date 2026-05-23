#pragma once

#include <QWidget>
#include <QTimer>
#include <QComboBox>
#include <QLabel>
#include <QTextEdit>
#include <QTableWidget>
#include <QPushButton>
#include "pipeline_manager.h"
#include "pipeline.h"
#include "widgets/i_tab_interfaces.h"

class OcrTabWidget : public QWidget, public ISignalConnectable, public IConfigurableTab, public IResultUpdatable
{
    Q_OBJECT

public:
    explicit OcrTabWidget(PipelineManager* pipelineManager, QWidget* parent = nullptr);
    ~OcrTabWidget();

    // IConfigurableTab 接口实现
    void saveToConfig(PipelineConfig& config) const override;
    void loadFromConfig(const PipelineConfig& config) override;

    // IResultUpdatable 接口实现
    void updateFromPipeline(const PipelineContext& ctx) override;

    void connectSignals(PipelineManager* pm, RoiManager* rm,
                        ImageView* view, RoiUiController* roiCtrl,
                        std::function<void()> requestRefresh,
                        std::function<void()> processAndDisplay) override;

signals:
    void ocrConfigChanged();
    void processRequested();

private slots:
    void onLanguageChanged(int index);
    void onRecognizeClicked();
    void onCopyResultClicked();
    void onResetClicked();

private:
    void setupUI();
    void syncConfigToPipeline();
    void updateRegionsTable(const QVector<OcrRegion>& regions);

    PipelineManager* m_pipelineManager;
    QTimer* m_debounceTimer;

    // UI 控件 - 配置部分
    QComboBox* m_languageCombo;
    QComboBox* m_pageModeCombo;
    QPushButton* m_recognizeBtn;
    QPushButton* m_copyBtn;
    QPushButton* m_resetBtn;

    // UI 控件 - 结果显示部分
    QLabel* m_hintLabel;
    QLabel* m_statusLabel;
    QTextEdit* m_resultTextEdit;
    QTableWidget* m_regionsTable;
};
