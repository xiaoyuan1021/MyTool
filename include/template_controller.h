#ifndef TEMPLATE_CONTROLLER_H
#define TEMPLATE_CONTROLLER_H

#include <QObject>
#include <QWidget>
#include <QMap>
#include <memory>
#include <opencv2/opencv.hpp>
#include "match_strategy.h"

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class ImageView;
class RoiManager;

enum class MatchType {
    ShapeModel,
    NCCModel,
    OpenCVTM
};

class TemplateController : public QObject
{
    Q_OBJECT

public:
    explicit TemplateController(Ui::MainWindow* ui,
                               ImageView* view,
                               RoiManager* roiManager,
                               QWidget* parent = nullptr);

    void initialize();

    // 策略管理
    void setMatchType(MatchType type);
    MatchType getCurrentMatchType() const { return m_currentType; }
    QString getCurrentStrategyName() const;

    // 模板操作
    bool hasTemplate() const;
    void clearTemplate();
    TemplateParams getDefaultParams() const { return m_defaultParams; }

public slots:
    void drawTemplate();
    void clearTemplateDrawing();
    void createTemplate();
    void findTemplate();
    void clearAllTemplates();
    void onMatchTypeChanged(int index);

signals:
    void imageToShow(const cv::Mat& image);
    void templateCreated(const QString& name);
    void matchCompleted(int count);

private:
    void updateUIState(bool hasTemplate);
    void createTemplateFromPolygon(const QVector<QPointF>& points, const QString& name);
    void initializeStrategies();

private:
    Ui::MainWindow* m_ui;
    ImageView* m_view;
    RoiManager* m_roiManager;
    QWidget* m_parent;

    // 策略管理
    std::shared_ptr<IMatchStrategy> m_currentStrategy;
    MatchType m_currentType;
    QMap<MatchType, std::shared_ptr<IMatchStrategy>> m_strategies;
    TemplateParams m_defaultParams;
};

#endif // TEMPLATE_CONTROLLER_H