#include "extract_tab_widget.h"
#include "ui_extract_tab.h"
#include "logger.h"
#include <QMessageBox>
#include <QListWidgetItem>

ExtractTabWidget::ExtractTabWidget(PipelineManager* pipeline,
                                   ImageView* view,
                                   RoiManager* roiManager,
                                   QWidget* parent)
    : QWidget(parent)
    , m_ui(new Ui::ExtractTabForm)
    , m_pipeline(pipeline)
    , m_view(view)
    , m_roiManager(roiManager)
{
    m_ui->setupUi(this);
}

ExtractTabWidget::~ExtractTabWidget()
{
    delete m_ui;
}

void ExtractTabWidget::initialize()
{
    setupConnections();

    connect(m_ui->listWidget_select, &QListWidget::itemClicked, this, [this](QListWidgetItem* item) {
        int index = item->data(Qt::UserRole).toInt();
        displayFilterCondition(index);
    });

    connect(m_ui->btn_clearRegion, &QPushButton::clicked, this, &ExtractTabWidget::clearRegion);
}

void ExtractTabWidget::setupConnections()
{
    connect(m_ui->comboBox_select, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &ExtractTabWidget::onSelectTypeChanged);

    connect(m_ui->comboBox_condition, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &ExtractTabWidget::onConditionChanged);

    connect(m_ui->btn_clearFilter, &QPushButton::clicked, this, &ExtractTabWidget::clearFilter);

    connect(m_ui->btn_addFilter, &QPushButton::clicked, this, &ExtractTabWidget::addFilter);

    connect(m_ui->btn_select, &QPushButton::clicked, this, &ExtractTabWidget::extractRegions);

    connect(m_ui->btn_drawRegion, &QPushButton::clicked, this, &ExtractTabWidget::drawRegion);
}

void ExtractTabWidget::onSelectTypeChanged(int index)
{
    if (!m_ui || !m_ui->lineEdit_minArea || !m_ui->lineEdit_maxArea) return;

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

void ExtractTabWidget::onConditionChanged(int index)
{
    if (!m_pipeline) return;

    FilterMode mode = (index == 0) ? FilterMode::And : FilterMode::Or;
    m_pipeline->setFilterMode(mode);

    QString modeName = (mode == FilterMode::And) ? "AND" : "OR";
    Logger::instance()->info(QString("筛选模式已切换:%1").arg(modeName));
}

void ExtractTabWidget::clearFilter()
{
    if (m_currentSelectedIndex < 0 || m_currentSelectedIndex >= m_filterConditions.size()) {
        Logger::instance()->warning("请先选择要删除的条件");
        return;
    }

    // 从本地列表中移除
    m_filterConditions.removeAt(m_currentSelectedIndex);

    // 清空 Pipeline 中的所有条件
    m_pipeline->clearShapeFilter();

    // 重新添加剩余的条件到 Pipeline
    for (const FilterCondition& condition : m_filterConditions) {
        m_pipeline->addFilterCondition(condition);
    }

    // 清空输入框
    if (m_ui) {
        if (m_ui->lineEdit_minArea) m_ui->lineEdit_minArea->clear();
        if (m_ui->lineEdit_maxArea) m_ui->lineEdit_maxArea->clear();
    }

    // 更新列表显示
    updateFilterListWidget();
    m_currentSelectedIndex = -1;

    emit extractionChanged();
    Logger::instance()->info("已删除选中的筛选条件");
}

void ExtractTabWidget::addFilter()
{
    if (!m_ui || !m_pipeline) return;

    const PipelineContext& ctx = m_pipeline->getLastContext();
    if (ctx.processed.empty()) {
        QMessageBox::warning(this, "提示", "请先执行算法处理!");
        return;
    }

    if (!m_ui->lineEdit_minArea || !m_ui->lineEdit_maxArea) return;

    bool ok1, ok2;
    double minValue = m_ui->lineEdit_minArea->text().toDouble(&ok1);
    double maxValue = m_ui->lineEdit_maxArea->text().toDouble(&ok2);

    if (!ok1 || !ok2 || minValue < 0 || maxValue < minValue) {
        QMessageBox::warning(this, "输入错误", "请输入有效的范围!");
        return;
    }

    if (!m_ui->comboBox_select) return;

    int featureIndex = m_ui->comboBox_select->currentIndex();
    ShapeFeature feature = static_cast<ShapeFeature>(featureIndex);

    FilterCondition condition(feature, minValue, maxValue);
    m_pipeline->addFilterCondition(condition);

    // 保存条件到本地列表
    m_filterConditions.append(condition);

    // 更新列表显示
    updateFilterListWidget();

    // 自动执行处理
    extractRegions();

    Logger::instance()->info(QString("已应用筛选: %1").arg(condition.toString()));
}

void ExtractTabWidget::extractRegions()
{
    m_view->clearPolygon();
    emit extractionChanged();
    Logger::instance()->info("区域已提取");
}

void ExtractTabWidget::drawRegion()
{
    if (m_roiManager->getCurrentImage().empty()) {
        Logger::instance()->warning("请先打开图像");
        return;
    }
    m_view->startPolygonDrawing("region");
    Logger::instance()->info("请在图像上点击左键添加顶点，右键完成绘制");
}

void ExtractTabWidget::clearRegion()
{
    m_view->clearPolygonDrawing();
    Logger::instance()->info("已清除绘制区域");
}

void ExtractTabWidget::calculateRegionFeatures(const QVector<QPointF>& points)
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

void ExtractTabWidget::updateFilterListWidget()
{
    if (!m_ui || !m_ui->listWidget_select) return;

    m_ui->listWidget_select->clear();
    for (int i = 0; i < m_filterConditions.size(); ++i) {
        const FilterCondition& condition = m_filterConditions[i];
        // 只显示条件类型名称，不显示具体数值
        QString featureName;
        switch(condition.feature) {
        case ShapeFeature::Area: featureName = "面积"; break;
        case ShapeFeature::Circularity: featureName = "圆度"; break;
        case ShapeFeature::Width: featureName = "宽度"; break;
        case ShapeFeature::Height: featureName = "高度"; break;
        default: featureName = "未知"; break;
        }
        QListWidgetItem* item = new QListWidgetItem(featureName);
        item->setData(Qt::UserRole, i);
        m_ui->listWidget_select->addItem(item);
    }
}

void ExtractTabWidget::onFilterListItemClicked(QListWidgetItem* item)
{
    Q_UNUSED(item);
    // 这个函数现在不再使用，因为 QListView 需要用 clicked 信号
    // 改用 Model 的方式处理
}

void ExtractTabWidget::displayFilterCondition(int index)
{
    if (!m_ui || index < 0 || index >= m_filterConditions.size()) return;

    m_currentSelectedIndex = index;
    const FilterCondition& condition = m_filterConditions[index];

    // 更新 comboBox_select 显示当前条件的类型
    if (m_ui->comboBox_select) {
        m_ui->comboBox_select->setCurrentIndex(static_cast<int>(condition.feature));
    }

    // 更新输入框显示最小值和最大值
    if (m_ui->lineEdit_minArea) {
        m_ui->lineEdit_minArea->setText(QString::number(condition.minValue));
    }
    if (m_ui->lineEdit_maxArea) {
        m_ui->lineEdit_maxArea->setText(QString::number(condition.maxValue));
    }
}