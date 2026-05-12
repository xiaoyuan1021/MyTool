#pragma once

#include <QWidget>
#include <functional>
#include <opencv2/core.hpp>
#include "pipeline_manager.h"
#include "config/pipeline_config.h"
#include "widgets/i_signal_connectable.h"

namespace Ui
{
class LineTabForm;
}

class LineDetectTabWidget : public QWidget, public ISignalConnectable
{
    Q_OBJECT

public:
    explicit LineDetectTabWidget(PipelineManager* pipelineManager,
                                 std::function<void()> processCallback,
                                 QWidget* parent = nullptr);
    ~LineDetectTabWidget();
    void initialize();

    void connectSignals(PipelineManager* pm, RoiManager* rm,
                        ImageView* view, RoiUiController* roiCtrl,
                        std::function<void()> requestRefresh,
                        std::function<void()> processAndDisplay) override;

private:
    PipelineManager* m_pipelineManager;
    std::function<void()> m_processCallback;
    std::function<void()> m_requestRefresh;
    Ui::LineTabForm* m_ui;

    // 复用 LineDetectConfig 类型定义（独立实例，不影响其他 Config）
    using LineDetectState = LineDetectConfig;
    LineDetectState getLineState() const;
    void setLineState(const LineDetectState& state);

private slots:
    void onLineAlgorithmChanged(int index);
    void onLineThresholdChanged(int value);
    void onLineMinLengthChanged(int value);
    void onLineMaxGapChanged(int value);
    void handleApply();
    
    // 参考线匹配槽函数
    void onReferenceLineEnabledChanged(bool enabled);
    void onDrawReferenceLineClicked();
    void onClearReferenceLineClicked();
    void onAngleThresholdChanged(double value);
    void onDistanceThresholdChanged(double value);
    void onSearchRegionWidthChanged(int value);

public:
    // 更新参考线状态显示
    void updateReferenceLineStatus();
    
    // 更新匹配结果状态
    void updateMatchResultStatus(int matchedCount, int totalCount, double angleThreshold, double distanceThreshold, bool isReferenceMode = false);
    
    // 设置参考线（供ImageView调用）
    void setReferenceLine(const cv::Point2f& start, const cv::Point2f& end);

signals:
    // 请求绘制参考线
    void requestDrawReferenceLine();
    
    // 请求清除参考线
    void requestClearReferenceLine();
};
