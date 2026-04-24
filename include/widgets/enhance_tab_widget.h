# pragma once

#include <QWidget>
#include "pipeline_manager.h"
#include "pipeline.h"
#include <QObject>
#include <QStack>
#include <QTimer>
#include <QSignalBlocker>
#include <functional>
#include "ui_enhance_tab.h"


namespace Ui {
class EnhanceTabWidget;
}

class EnhanceTabWidget : public QWidget {
    Q_OBJECT

public:
    explicit EnhanceTabWidget(PipelineManager* pipelineManager, QWidget* parent = nullptr);
    ~EnhanceTabWidget();

    // 配置管理
    void getEnhanceConfig(int& brightness, int& contrast, int& gamma, int& sharpen) const;
    void setEnhanceConfig(int brightness, int contrast, int gamma, int sharpen);

signals:
    void brightnessChanged(int);
    void contrastChanged(int);
    void gammaChanged(int);
    void sharpenChanged(int);
    void processRequested();
    /** 增强参数变化时触发，用于通知外部将配置回写到当前ROI */
    void enhanceConfigChanged();

private slots:

    void on_Slider_brightness_valueChanged(int value);
    void on_Slider_contrast_valueChanged(int value);
    void on_Slider_gamma_valueChanged(int value);
    void on_Slider_sharpen_valueChanged(int value);
    void on_btn_resetBC_clicked();
    void on_btn_saveBC_clicked();
    void on_btn_undoBC_clicked();

private:
    struct EnhancementState 
    { 
        int brightness = 0;
        int contrast = 100;
        int gamma = 100;
        int sharpen = 100;

        bool operator==(const EnhancementState& other) const
        {
            return brightness == other.brightness &&
                contrast == other.contrast &&
                gamma == other.gamma &&
                sharpen == other.sharpen;
        }   
    };
    EnhancementState captureState() const;
    void applyState(const EnhancementState& state);
    void pushSnapshot(const EnhancementState& state);
    void updateUndoUi();
    void syncConfigToPipeline();
    
    QTimer* m_debounceTimer;

    QStack<EnhancementState> m_enhancementHistory;
    Ui::EnhanceTabWidget* m_ui;
    PipelineManager* m_pipelineManager;
};