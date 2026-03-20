#pragma once

#include <QWidget>
#include "pipeline_manager.h"
#include "image_view.h"
#include "halcon_algorithm.h"
#include <QListWidgetItem>

namespace Ui
{
class ExtractTabForm;
}

class ExtractTabWidget : public QWidget
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
    //void getFilterConditions(QVector<FilterCondition>& conditions) const;

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