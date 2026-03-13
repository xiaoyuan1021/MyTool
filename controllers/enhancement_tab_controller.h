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
    ~EnhancementTabController();

    void initialize();

private slots:
    void handleBrightnessChanged(int value);
    void handleContrastChanged(int value);
    void handleGammaChanged(int value);
    void handleSharpenChanged(int value);

    //void handleReset();
    //void handleSave();
    //void handleUndo();

private:
    
    Ui::MainWindow* m_ui = nullptr;
    PipelineManager* m_pipeline = nullptr;
    QTimer* m_debounceTimer = nullptr;
    std::function<void()> m_processCallback;
    bool m_grayFilterForced = false;   // 可选，用来记录灰度开关
};
