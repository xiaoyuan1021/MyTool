#pragma once

#include <QWidget>
#include <QComboBox>
#include <QSlider>
#include <QLabel>
#include <QStack>
#include "pipeline_manager.h"
#include "pipeline.h"
#include "widgets/i_tab_interfaces.h"

class ImageFilterTabWidget : public QWidget, public ISignalConnectable, public IConfigurableTab
{
    Q_OBJECT

public:
    explicit ImageFilterTabWidget(PipelineManager* pipelineManager, QWidget* parent = nullptr);
    ~ImageFilterTabWidget();

    // IConfigurableTab 接口实现
    void saveToConfig(PipelineConfig& config) const override;
    void loadFromConfig(const PipelineConfig& config) override;

    void connectSignals(const SignalContext& ctx,
                        std::function<void()> onExecutePipeline,
                        std::function<void()> onConfigSaved = nullptr) override;

signals:
    void filterConfigChanged();
    void processRequested();

private slots:
    void onFilterTypeChanged(int index);
    void onMorphOpChanged(int index);
    void onResetClicked();

private:
    void setupUI();
    void updateParamVisibility();
    void syncConfigToPipeline();
    void applyConfig();

    PipelineManager* m_pipelineManager;

    // UI 控件
    QComboBox* m_filterTypeCombo;
    QSlider* m_kernelSizeSlider;
    QLabel* m_kernelSizeLabel;
    QSlider* m_sigmaXSlider;
    QLabel* m_sigmaXLabel;
    QSlider* m_sigmaYSlider;
    QLabel* m_sigmaYLabel;
    QSlider* m_medianKernelSlider;
    QLabel* m_medianKernelLabel;
    QSlider* m_bilateralDSlider;
    QLabel* m_bilateralDLabel;
    QSlider* m_bilateralSigmaCSlider;
    QLabel* m_bilateralSigmaCLabel;
    QSlider* m_bilateralSigmaSSlider;
    QLabel* m_bilateralSigmaSLabel;
    QComboBox* m_morphOpCombo;
    QSlider* m_morphKernelSlider;
    QLabel* m_morphKernelLabel;
    QSlider* m_morphIterSlider;
    QLabel* m_morphIterLabel;

    // 参数面板容器
    QWidget* m_gaussianParams;
    QWidget* m_medianParams;
    QWidget* m_bilateralParams;
    QWidget* m_morphologyParams;
};
