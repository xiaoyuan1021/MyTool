#pragma once

#include <QWidget>
#include <QTimer>
#include "pipeline_manager.h"
#include "widgets/i_signal_connectable.h"

namespace Ui {
class FilterTabWidget;
}

class FilterTabWidget : public QWidget, public ISignalConnectable {
    Q_OBJECT

public:
    explicit FilterTabWidget(PipelineManager* pipelineManager, QWidget* parent = nullptr);
    ~FilterTabWidget();

    void getFilterConfig(int& mode, int& grayLow, int& grayHigh) const;
    void setFilterConfig(int mode, int grayLow, int grayHigh);

    void connectSignals(PipelineManager* pm, RoiManager* rm,
                        ImageView* view, RoiUiController* roiCtrl,
                        std::function<void()> requestRefresh,
                        std::function<void()> processAndDisplay) override;

signals:
    void filterConfigChanged();

private slots:
    void onSelectTypeChanged(int index);
    void onConditionChanged(int index);
    void onFilterModeChanged(int index);
    void filterColorChannelsChanged();

private:
    void setupConnections();
    void syncGrayParameters();
    void syncRGBParameters();
    void syncHSVParameters();

private:
    Ui::FilterTabWidget* m_ui;
    PipelineManager* m_pipelineManager;
    QTimer* m_debounceTimer;
};
