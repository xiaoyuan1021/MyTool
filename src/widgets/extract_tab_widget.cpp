#include "extract_tab_widget.h"
#include "ui_extract_tab.h"
#include "algorithm/opencv_algorithm.h"
#include "logger.h"
#include "config/config_manager.h"
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

    // 连接矩形绘制完成信号
    connect(m_view, &ImageView::polygonDrawingFinished, this, [this](const QString &type, QVector<QPointF> points) {
        if (type == "region") {
            calculateRegionFeatures(points);
        }
    });
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

    ShapeFeature feature = static_cast<ShapeFeature>(index);

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

    // 查找列表中是否已有该类型的筛选条件，如有则自动选中
    for (int i = 0; i < m_filterConditions.size(); ++i) {
        if (m_filterConditions[i].feature == feature) {
            m_currentSelectedIndex = i;
            if (m_ui->listWidget_select) {
                m_ui->listWidget_select->setCurrentRow(i);
            }
            calculateAndShowRange(feature);
            return;
        }
    }

    // 没有找到匹配项，只显示范围
    calculateAndShowRange(feature);
}

void ExtractTabWidget::onConditionChanged(int index)
{
    if (!m_pipeline) return;

    auto mode = (index == 0) ? ShapeFilterLogicMode::And : ShapeFilterLogicMode::Or;
    m_pipeline->mutableConfig().shapeFilter.mode = mode;

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

    // 清空输入框
    if (m_ui) {
        if (m_ui->lineEdit_minArea) m_ui->lineEdit_minArea->clear();
        if (m_ui->lineEdit_maxArea) m_ui->lineEdit_maxArea->clear();
    }

    // 更新列表显示
    updateFilterListWidget();
    m_currentSelectedIndex = -1;

    // 更新范围显示
    if (m_ui && m_ui->comboBox_select) {
        ShapeFeature feature = static_cast<ShapeFeature>(m_ui->comboBox_select->currentIndex());
        calculateAndShowRange(feature);
    }

    Logger::instance()->info("已删除选中的筛选条件");
}

void ExtractTabWidget::addFilter()
{
    if (!m_ui || !m_pipeline) return;

    const PipelineContext& ctx = m_pipeline->getLastContext();
    
    cv::Mat inputMat = !ctx.extractedMask.empty() ? ctx.extractedMask : ctx.combinedMask;
    
    if (inputMat.empty()) {
        QMessageBox::warning(this, "提示", "请先执行算法处理!");
        return;
    }

    if (!m_ui->comboBox_select) return;

    int featureIndex = m_ui->comboBox_select->currentIndex();
    ShapeFeature feature = static_cast<ShapeFeature>(featureIndex);

    // 检查是否已存在同类型筛选条件
    for (const FilterCondition& cond : m_filterConditions) {
        if (cond.feature == feature) {
            QMessageBox::warning(this, "提示", "该类型的筛选条件已存在!");
            return;
        }
    }

    // 创建默认条件（不验证min/max输入）
    FilterCondition condition(feature, 0.0, 1e18);
    m_filterConditions.append(condition);

    // 更新列表显示
    updateFilterListWidget();

    // 自动选中新添加的项
    int newIndex = m_filterConditions.size() - 1;
    m_currentSelectedIndex = newIndex;
    if (m_ui->listWidget_select) {
        m_ui->listWidget_select->setCurrentRow(newIndex);
    }

    // 计算并显示特征范围
    calculateAndShowRange(feature);

    Logger::instance()->info(QString("已添加筛选条件: %1").arg(getFeatureDisplayName(feature)));
}

void ExtractTabWidget::extractRegions()
{
    if (!m_pipeline) return;

    // 先保存当前正在编辑的条件
    saveCurrentFilterCondition();

    // 同步所有条件到Pipeline
    m_pipeline->mutableConfig().shapeFilter.clear();
    for (const FilterCondition& condition : m_filterConditions) {
        if (condition.isValid()) {
            m_pipeline->mutableConfig().shapeFilter.addCondition(condition);
        }
    }

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
    //m_view->startPolygonDrawing("region");
    m_view->startRectangleDrawing("region");

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
    if (ctx.extractedMask.empty() && ctx.combinedMask.empty()) {
        Logger::instance()->warning("请先执行算法处理，然后再绘制区域");
        return;
    }

    cv::Mat processedImg = !ctx.extractedMask.empty() ? ctx.extractedMask : ctx.combinedMask;
    OpenCVAlgorithm analyzer;
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

void ExtractTabWidget::calculateAndShowRange(ShapeFeature feature)
{
    if (!m_ui || !m_pipeline) return;

    const PipelineContext& ctx = m_pipeline->getLastContext();
    cv::Mat inputMat = !ctx.extractedMask.empty() ? ctx.extractedMask : ctx.combinedMask;

    if (inputMat.empty()) {
        if (m_ui->label_featureRange) {
            m_ui->label_featureRange->setText("当前范围: --");
        }
        return;
    }

    const char* featureName = getFeatureName(feature);
    OpenCVAlgorithm::FeatureRange range = OpenCVAlgorithm::calculateFeatureRange(inputMat, featureName);

    QString featureDisplayName = getFeatureDisplayName(feature);
    QString text;
    if (range.regionCount > 0) {
        text = QString("%1 最小值: %2 最大值: %3<br>共%4个区域")
            .arg(featureDisplayName)
            .arg(range.minValue, 0, 'f', 1)
            .arg(range.maxValue, 0, 'f', 1)
            .arg(range.regionCount);
    } else {
        text = QString("%1 无数据").arg(featureDisplayName);
    }

    if (m_ui->label_featureRange) {
        m_ui->label_featureRange->setTextFormat(Qt::RichText);
        m_ui->label_featureRange->setText(text);
    }
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

    // 先保存前一个条件的修改
    saveCurrentFilterCondition();

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

    // 计算并显示特征范围
    calculateAndShowRange(condition.feature);
}

void ExtractTabWidget::getExtractConfig(ShapeFilterConfig& config) const
{
    config.conditions = m_filterConditions;
    config.mode = (m_ui && m_ui->comboBox_condition) ?
                  ((m_ui->comboBox_condition->currentIndex() == 0) ? FilterMode::And : FilterMode::Or) :
                  FilterMode::And;
}

void ExtractTabWidget::setExtractConfig(const ShapeFilterConfig& config)
{
    if (!m_ui || !m_pipeline) return;

    // 清空当前条件
    m_filterConditions.clear();
    m_pipeline->mutableConfig().shapeFilter.clear();

    // 应用新配置
    m_filterConditions = config.conditions;
    m_pipeline->mutableConfig().shapeFilter = config;

    // 更新UI显示
    updateFilterListWidget();

    // 更新模式选择
    if (m_ui->comboBox_condition) {
        m_ui->comboBox_condition->setCurrentIndex(config.mode == FilterMode::And ? 0 : 1);
    }

    Logger::instance()->info(QString("已加载提取配置: %1 个条件").arg(config.conditions.size()));
}

void ExtractTabWidget::saveCurrentFilterCondition()
{
    if (m_currentSelectedIndex < 0 || m_currentSelectedIndex >= m_filterConditions.size()) return;
    if (!m_ui || !m_pipeline) return;

    // 从输入框读取用户编辑的值
    bool ok1, ok2;
    double minValue = m_ui->lineEdit_minArea->text().toDouble(&ok1);
    double maxValue = m_ui->lineEdit_maxArea->text().toDouble(&ok2);

    // 验证输入有效性
    if (!ok1 || !ok2 || minValue < 0 || maxValue < minValue) {
        return;  // 输入无效，不保存
    }

    // 更新本地列表中的条件
    m_filterConditions[m_currentSelectedIndex].minValue = minValue;
    m_filterConditions[m_currentSelectedIndex].maxValue = maxValue;

    // 重新同步到 Pipeline
    m_pipeline->mutableConfig().shapeFilter.clear();
    for (const FilterCondition& condition : m_filterConditions) {
        m_pipeline->mutableConfig().shapeFilter.addCondition(condition);
    }

    // 触发重新处理
    emit extractionChanged();
}

void ExtractTabWidget::connectSignals(PipelineManager* pm, RoiManager* rm,
                                      ImageView* view, RoiUiController* roiCtrl,
                                      std::function<void()> onConfigChanged,
                                      std::function<void()> onExecuteRequested)
{
    Q_UNUSED(pm); Q_UNUSED(rm); Q_UNUSED(view); Q_UNUSED(roiCtrl); Q_UNUSED(onExecuteRequested);
    connect(this, &ExtractTabWidget::extractionChanged,
            this, [onConfigChanged]() { onConfigChanged(); });
}

// ========== IConfigurableTab 接口实现 ==========

void ExtractTabWidget::saveToConfig(PipelineConfig& config) const
{
    getExtractConfig(config.shapeFilter);
}

void ExtractTabWidget::loadFromConfig(const PipelineConfig& config)
{
    setExtractConfig(config.shapeFilter);
}

// ========== IResultUpdatable 接口实现 ==========

void ExtractTabWidget::updateFromPipeline(const PipelineContext& ctx)
{
    // 管道执行完成后，更新筛选结果到QLabel
    // 只有当有筛选条件时才更新（reason非空表示筛选执行了）
    if (!ctx.reason.isEmpty()) {
        // 使用筛选后的图像计算特征范围
        cv::Mat filteredImg = ctx.extractedMask;
        if (!filteredImg.empty() && m_ui && m_ui->comboBox_select) {
            int featureIndex = m_ui->comboBox_select->currentIndex();
            ShapeFeature feature = static_cast<ShapeFeature>(featureIndex);
            const char* featureName = getFeatureName(feature);
            OpenCVAlgorithm::FeatureRange range = OpenCVAlgorithm::calculateFeatureRange(filteredImg, featureName);
            updateRangeLabelWithResult(range);
        }
    }
}

void ExtractTabWidget::updateRangeLabelWithResult(const OpenCVAlgorithm::FeatureRange& filteredRange)
{
    if (!m_ui || !m_ui->label_featureRange) return;

    // 获取当前label文本，在后面追加筛选结果
    QString currentText = m_ui->label_featureRange->text();
    // 移除之前可能添加的筛选结果行
    int pos = currentText.indexOf("<br>筛选后");
    if (pos >= 0) {
        currentText = currentText.left(pos);
    }

    // 获取当前特征名称
    QString featureDisplayName = "--";
    if (m_ui->comboBox_select) {
        featureDisplayName = getFeatureDisplayName(static_cast<ShapeFeature>(m_ui->comboBox_select->currentIndex()));
    }

    // 构建筛选结果文本
    QString resultText;
    if (filteredRange.regionCount > 0) {
        resultText = QString("<br>筛选后 %1 最小值: %2 最大值: %3 共%4个区域")
            .arg(featureDisplayName)
            .arg(filteredRange.minValue, 0, 'f', 1)
            .arg(filteredRange.maxValue, 0, 'f', 1)
            .arg(filteredRange.regionCount);
    } else {
        resultText = QString("<br>筛选后 无匹配区域");
    }

    m_ui->label_featureRange->setText(currentText + resultText);
}
