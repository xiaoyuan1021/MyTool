#pragma once

#include <QWidget>
#include <functional>
#include "pipeline_manager.h"

namespace Ui
{
class LineTabForm;
}

class LineDetectTabWidget : public QWidget
{
    Q_OBJECT

public:
    explicit LineDetectTabWidget(PipelineManager* pipelineManager,
                                 std::function<void()> processCallback,
                                 QWidget* parent = nullptr);
    ~LineDetectTabWidget();
    void initialize();

private:
    PipelineManager* m_pipelineManager;
    std::function<void()> m_processCallback;
    Ui::LineTabForm* m_ui;

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
    void onLineAlgorithmChanged(int index);
    void onLineThresholdChanged(int value);
    void onLineMinLengthChanged(int value);
    void onLineMaxGapChanged(int value);
    void handleApply();
};
