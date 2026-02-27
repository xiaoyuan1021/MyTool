#pragma once
#include <QObject>
#include <QStack>
#include <QTimer>
#include <QSignalBlocker>
#include <functional>
#include "ui_mainwindow.h"
#include "pipeline_manager.h"

class EnhancementTabController : public QObject {
    Q_OBJECT
public:
    EnhancementTabController(Ui::MainWindow* ui,
                             PipelineManager* pipeline,
                             QTimer* debounceTimer,
                             std::function<void()> processCallback,
                             QObject* parent = nullptr);

    void initialize();

private slots:
    void handleBrightnessChanged(int value);
    void handleContrastChanged(int value);
    void handleGammaChanged(int value);
    void handleSharpenChanged(int value);

    void handleReset();
    void handleSave();
    void handleUndo();

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

    QStack<EnhancementState> m_enhancementHistory;
    Ui::MainWindow* m_ui = nullptr;
    PipelineManager* m_pipeline = nullptr;
    QTimer* m_debounceTimer = nullptr;
    std::function<void()> m_processCallback;
    bool m_grayFilterForced = false;   // 可选，用来记录灰度开关
};
