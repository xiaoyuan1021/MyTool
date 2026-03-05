#ifndef TEMPLATE_UI_CONTROLLER_H
#define TEMPLATE_UI_CONTROLLER_H

#include <QObject>
#include <QWidget>
#include <opencv2/opencv.hpp>
#include "template_match_manager.h"

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class ImageView;
class RoiManager;

class TemplateUIController : public QObject
{
    Q_OBJECT

public:
    explicit TemplateUIController(Ui::MainWindow* ui,
                                 TemplateMatchManager* templateManager,
                                 ImageView* view,
                                 RoiManager* roiManager,
                                 QWidget* parent = nullptr);

    void initialize();

public slots:
    void drawTemplate();
    void clearTemplate();
    void createTemplate();
    void findTemplate();
    void clearAllTemplates();
    void onMatchTypeChanged(int index);

signals:
    void imageToShow(const cv::Mat& image);

private:
    void updateUIState(bool hasTemplate);
    void createTemplateFromPolygon(const QVector<QPointF>& points, const QString& name);

private:
    Ui::MainWindow* m_ui;
    TemplateMatchManager* m_templateManager;
    ImageView* m_view;
    RoiManager* m_roiManager;
    QWidget* m_parent;
};

#endif // TEMPLATE_UI_CONTROLLER_H