#pragma once

#include <QWidget>
#include <QComboBox>
#include <QLabel>
#include <QTextEdit>
#include <QTableWidget>
#include <QPushButton>
#include <QLineEdit>
#include <QCheckBox>
#include "core/i_pipeline_access.h"
#include "widgets/i_tab_interfaces.h"

class OcrTabWidget : public QWidget, public ISignalConnectable, public IConfigurableTab, public IResultUpdatable
{
    Q_OBJECT

public:
    explicit OcrTabWidget(IPipelineAccess* pipelineAccess, QWidget* parent = nullptr);
    ~OcrTabWidget();

    // IConfigurableTab 接口实现
    void saveToConfig(PipelineConfig& config) const override;
    void loadFromConfig(const PipelineConfig& config) override;

    // IResultUpdatable 接口实现
    void updateFromPipeline(const PipelineContext& ctx) override;

    void connectSignals(const SignalContext& ctx,
                        std::function<void()> onExecutePipeline,
                        std::function<void()> onConfigSaved = nullptr) override;

signals:
    void processRequested();

private slots:
    void onRecognizeClicked();
    void onCopyResultClicked();
    void onResetClicked();

private:
    void setupUI();
    void syncConfigToPipeline();
    void clearResults();
    void updateRegionsTable(const QVector<OcrRegion>& regions);

    IPipelineAccess* m_pipeline;

    bool m_manualOcrTrigger = false;

    // UI 控件 - 配置部分
    QComboBox* m_pageModeCombo;
    QLineEdit* m_expectedTextLineEdit;
    QCheckBox* m_matchExactCheckBox;
    QPushButton* m_recognizeBtn;
    QPushButton* m_copyBtn;
    QPushButton* m_resetBtn;

    // UI 控件 - 结果显示部分
    QLabel* m_hintLabel;
    QLabel* m_statusLabel;
    QTextEdit* m_resultTextEdit;
    QTableWidget* m_regionsTable;
};
