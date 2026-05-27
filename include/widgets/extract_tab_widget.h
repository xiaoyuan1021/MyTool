#pragma once

#include <QWidget>
#include "pipeline_manager.h"
#include "image_view.h"
#include <QListWidgetItem>
#include "widgets/i_tab_interfaces.h"
#include "algorithm/opencv_algorithm.h"

namespace Ui
{
class ExtractTabForm;
}

class QStatusBar;

class ExtractTabWidget : public QWidget, public ISignalConnectable, public IConfigurableTab, public IResultUpdatable
{
    Q_OBJECT
public:
    explicit ExtractTabWidget(PipelineManager* pipeline,
                              ImageView* view,
                              RoiManager* roiManager,
                              QStatusBar* statusBar = nullptr,
                              QWidget* parent = nullptr);
    ~ExtractTabWidget();
    void initialize();
    void calculateRegionFeatures(const QVector<QPointF>& points);
    void getExtractConfig(ShapeFilterConfig& config) const;
    void setExtractConfig(const ShapeFilterConfig& config);
    void saveCurrentFilterCondition();

    // IConfigurableTab 接口实现
    void saveToConfig(PipelineConfig& config) const override;
    void loadFromConfig(const PipelineConfig& config) override;

    // IResultUpdatable 接口实现
    void updateFromPipeline(const PipelineContext& ctx) override;

    void connectSignals(PipelineManager* pm, RoiManager* rm,
                        ImageView* view, RoiUiController* roiCtrl,
                        std::function<void()> onConfigChanged,
                        std::function<void()> onExecuteRequested) override;
    
signals:
    void extractionChanged();

private slots:
    void onSelectTypeChanged(int index);
    void onConditionChanged(int index);
    void clearFilter();
    void addFilter();
    void extractRegions();
    void drawRegion();
    void clearRegion();
    void onFilterListItemClicked(QListWidgetItem* item);

private:
    void setupConnections();
    void updateFilterListWidget();
    void displayFilterCondition(int index);
    void calculateAndShowRange(ShapeFeature feature);
    void updateRangeLabelWithResult(const OpenCVAlgorithm::FeatureRange& filteredRange);

private:
    Ui::ExtractTabForm* m_ui;
    PipelineManager* m_pipeline;
    ImageView* m_view;
    RoiManager* m_roiManager;
    QStatusBar* m_statusBar;
    QVector<FilterCondition> m_filterConditions;
    int m_currentSelectedIndex = -1;
};