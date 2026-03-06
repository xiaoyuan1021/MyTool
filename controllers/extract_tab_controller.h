#pragma once

#include <QObject>
#include "ui_mainwindow.h"
#include "pipeline_manager.h"
#include "image_view.h"
#include "halcon_algorithm.h"

class ExtractTabController : public QObject
{
    Q_OBJECT
public:
    ExtractTabController(Ui::MainWindow* ui, PipelineManager* pipeline,
                        ImageView* view, RoiManager* roiManager, QObject* parent = nullptr);
    void initialize();
    void calculateRegionFeatures(const QVector<QPointF>& points);

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

private:
    void setupConnections();
    //void calculateRegionFeatures(const QVector<QPointF>& points);

private:
    Ui::MainWindow* m_ui;
    PipelineManager* m_pipeline;
    ImageView* m_view;
    RoiManager* m_roiManager;
};