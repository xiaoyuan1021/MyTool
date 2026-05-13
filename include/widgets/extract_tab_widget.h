#pragma once

#include <QWidget>
#include "pipeline_manager.h"
#include "image_view.h"
#include <QListWidgetItem>
#include "widgets/i_tab_interfaces.h"

namespace Ui
{
class ExtractTabForm;
}

class ExtractTabWidget : public QWidget, public ISignalConnectable, public IConfigurableTab
{
    Q_OBJECT
public:
    explicit ExtractTabWidget(PipelineManager* pipeline,
                              ImageView* view,
                              RoiManager* roiManager,
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

    void connectSignals(PipelineManager* pm, RoiManager* rm,
                        ImageView* view, RoiUiController* roiCtrl,
                        std::function<void()> requestRefresh,
                        std::function<void()> processAndDisplay) override;
    
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

private:
    Ui::ExtractTabForm* m_ui;
    PipelineManager* m_pipeline;
    ImageView* m_view;
    RoiManager* m_roiManager;
    QVector<FilterCondition> m_filterConditions;
    int m_currentSelectedIndex = -1;
};