#include "extract_tab_controller.h"
#include "logger.h"
#include <QMessageBox>

ExtractTabController::ExtractTabController(Ui::MainWindow* ui, PipelineManager* pipeline,
                                         ImageView* view, RoiManager* roiManager, QObject* parent)
    : QObject(parent), m_ui(ui), m_pipeline(pipeline), m_view(view), m_roiManager(roiManager)
{
}

void ExtractTabController::initialize()
{
    setupConnections();
}

void ExtractTabController::setupConnections()
{
    connect(m_ui->comboBox_select, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &ExtractTabController::onSelectTypeChanged);
    connect(m_ui->comboBox_condition, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &ExtractTabController::onConditionChanged);
    connect(m_ui->btn_clearFilter, &QPushButton::clicked, this, &ExtractTabController::clearFilter);
    connect(m_ui->btn_addFilter, &QPushButton::clicked, this, &ExtractTabController::addFilter);
    connect(m_ui->btn_select, &QPushButton::clicked, this, &ExtractTabController::extractRegions);
    connect(m_ui->btn_drawRegion, &QPushButton::clicked, this, &ExtractTabController::drawRegion);
    connect(m_ui->btn_clearRegion, &QPushButton::clicked, this, &ExtractTabController::clearRegion);
}

void ExtractTabController::onSelectTypeChanged(int index)
{
    switch(index) {
    case 0: // 面积
        m_ui->lineEdit_minArea->setPlaceholderText("例如: 50");
        m_ui->lineEdit_maxArea->setPlaceholderText("例如: 1000");
        break;
    case 1: // 圆度
        m_ui->lineEdit_minArea->setPlaceholderText("例如: 0.8");
        m_ui->lineEdit_maxArea->setPlaceholderText("例如: 1.0");
        break;
    case 2: // 宽度
        m_ui->lineEdit_minArea->setPlaceholderText("例如: 10");
        m_ui->lineEdit_maxArea->setPlaceholderText("例如: 100");
        break;
    case 3: // 高度
        m_ui->lineEdit_minArea->setPlaceholderText("例如: 10");
        m_ui->lineEdit_maxArea->setPlaceholderText("例如: 100");
        break;
    }
}

void ExtractTabController::onConditionChanged(int index)
{
    FilterMode mode = (index == 0) ? FilterMode::And : FilterMode::Or;
    m_pipeline->setFilterMode(mode);
    Logger::instance()->info(QString("筛选模式已切换:%1").arg(getFilterModeName(mode)));
}

void ExtractTabController::clearFilter()
{
    m_pipeline->clearShapeFilter();
    m_ui->lineEdit_minArea->clear();
    m_ui->lineEdit_maxArea->clear();
    emit extractionChanged();
    m_ui->statusbar->showMessage("已清除所有筛选条件", 2000);
}

void ExtractTabController::addFilter()
{
    const PipelineContext& ctx = m_pipeline->getLastContext();
    if (ctx.processed.empty()) {
        QMessageBox::warning(m_ui->centralwidget, "提示", "请先执行算法处理!");
        return;
    }

    bool ok1, ok2;
    double minValue = m_ui->lineEdit_minArea->text().toDouble(&ok1);
    double maxValue = m_ui->lineEdit_maxArea->text().toDouble(&ok2);

    if (!ok1 || !ok2 || minValue < 0 || maxValue < minValue) {
        QMessageBox::warning(m_ui->centralwidget, "输入错误", "请输入有效的范围!");
        return;
    }

    int featureIndex = m_ui->comboBox_select->currentIndex();
    ShapeFeature feature = static_cast<ShapeFeature>(featureIndex);

    FilterCondition condition(feature, minValue, maxValue);
    m_pipeline->addFilterCondition(condition);

    m_ui->statusbar->showMessage(QString("已应用筛选: %1").arg(condition.toString()), 2000);
    Logger::instance()->info(QString("已应用筛选: %1").arg(condition.toString()));
}

void ExtractTabController::extractRegions()
{
    m_view->clearPolygon();
    emit extractionChanged();
    Logger::instance()->info("区域已提取");
}

void ExtractTabController::drawRegion()
{
    if (m_roiManager->getCurrentImage().empty()) {
        Logger::instance()->warning("请先打开图像");
        return;
    }
    m_view->startPolygonDrawing("region");
    m_ui->statusbar->showMessage("请在图像上点击左键添加顶点，右键完成绘制");
}

void ExtractTabController::clearRegion()
{
    m_view->clearPolygonDrawing();
    m_ui->statusbar->showMessage("已清除绘制区域");
}

void ExtractTabController::calculateRegionFeatures(const QVector<QPointF>& points)
{
    if (points.size() < 3) {
        Logger::instance()->warning("顶点数量不足，至少需要3个点");
        return;
    }

    const PipelineContext& ctx = m_pipeline->getLastContext();
    if (ctx.processed.empty() && ctx.mask.empty()) {
        Logger::instance()->warning("请先执行算法处理，然后再绘制区域");
        return;
    }

    cv::Mat processedImg = ctx.processed.empty() ? ctx.mask : ctx.processed;
    HalconAlgorithm analyzer;
    QVector<RegionFeature> features = analyzer.analyzeRegionsInPolygon(points, processedImg);

    if (features.isEmpty()) return;

    Logger::instance()->info("========== ROI 区域特征分析 ==========");
    Logger::instance()->info(QString("找到 %1 个连通域").arg(features.size()));
    Logger::instance()->info("-----------------------------------");

    for (const auto& feature : features) {
        Logger::instance()->info(feature.toString());
    }
    Logger::instance()->info("======================================");
}