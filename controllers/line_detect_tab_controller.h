#pragma once

#include <QObject>
#include "ui_mainwindow.h"
#include "pipeline_manager.h"

class LineDetectTabController : public QObject
{
    Q_OBJECT
public:
    LineDetectTabController(Ui::MainWindow* ui,
                       PipelineManager* pipeline,
                       std::function<void()> processCallback,
                       QObject* parent = nullptr);

    void initialize();
signals:
    

private:
    struct LineDetectState 
    { 
        int algorithm = 0;  // 0=HoughP, 1=LSD, 2=EDline
        double rho = 1.0;
        double theta = CV_PI / 180.0;
        int threshold = 50;
        double minLineLength = 30.0;
        double maxLineGap = 10.0;

        bool operator==(const LineDetectState& other) const
        {
            return algorithm == other.algorithm &&
                rho == other.rho &&
                theta == other.theta &&
                threshold == other.threshold &&
                minLineLength == other.minLineLength &&
                maxLineGap == other.maxLineGap;
        }
    };
    LineDetectState getLineState() const;
    void setLineState(const LineDetectState& state);
private slots:
    void onLineAlgorithmChanged(int index);  // LSD/EDline/HoughP选择
    void onLineThresholdChanged(int value);
    void onLineMinLengthChanged(int value);
    void onLineMaxGapChanged(int value);
    void handleApply();

private:
    Ui::MainWindow* m_ui;
    PipelineManager* m_pipeline;
    std::function<void()> m_processCallback;

};
