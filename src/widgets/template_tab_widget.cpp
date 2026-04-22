#include "template_tab_widget.h"
#include "ui_template_tab.h"
#include "image_view.h"
#include "logger.h"
#include <QMessageBox>
#include <QInputDialog>
#include <QFileDialog>

TemplateTabWidget::TemplateTabWidget(ImageView* view,
                                     RoiManager* roiManager,
                                     QWidget* parent)
    : QWidget(parent)
    , m_ui(new Ui::TemplateTabWidget)
    , m_view(view)
    , m_roiManager(roiManager)
    , m_parent(parent)
    , m_currentType(MatchType::OpenCVTM)
{
    m_ui->setupUi(this);

    // 初始化默认参数
    m_defaultParams.matchMethod = cv::TM_CCOEFF_NORMED;
}

TemplateTabWidget::~TemplateTabWidget()
{
    delete m_ui;
}

void TemplateTabWidget::initialize()
{
    initializeStrategies();

    m_ui->doubleSpinBox_minScore->setValue(0.8);
    m_ui->doubleSpinBox_minScore->setSingleStep(0.1);
    m_ui->spinBox_matchNumber->setValue(1);

    // 连接信号槽
    connect(m_ui->btn_drawTemplate, &QPushButton::clicked, this, &TemplateTabWidget::drawTemplate);
    connect(m_ui->btn_clearTemplate, &QPushButton::clicked, this, &TemplateTabWidget::clearTemplateDrawing);
    connect(m_ui->btn_creatTemplate, &QPushButton::clicked, this, &TemplateTabWidget::createTemplate);
    connect(m_ui->btn_findTemplate, &QPushButton::clicked, this, &TemplateTabWidget::findTemplate);
    connect(m_ui->btn_saveTemplate, &QPushButton::clicked, this, &TemplateTabWidget::saveTemplate);
    connect(m_ui->btn_loadTemplate, &QPushButton::clicked, this, &TemplateTabWidget::loadTemplate);
    //connect(m_ui->btn_clearAllTemplates, &QPushButton::clicked, this, &TemplateTabWidget::clearAllTemplates);
    connect(m_ui->comboBox_matchType, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &TemplateTabWidget::onMatchTypeChanged);
}

void TemplateTabWidget::initializeStrategies()
{
    m_strategies[MatchType::OpenCVTM] = std::make_shared<OpenCVMatchStrategy>();
    m_currentStrategy = m_strategies[m_currentType];
}

void TemplateTabWidget::setMatchType(MatchType type)
{
    if (m_currentType != type) {
        m_currentType = type;
        m_currentStrategy = m_strategies[type];
    }
}

QString TemplateTabWidget::getCurrentStrategyName() const
{
    return m_currentStrategy ? m_currentStrategy->getStrategyName() : "Unknown";
}

bool TemplateTabWidget::hasTemplate() const
{
    return m_currentStrategy && m_currentStrategy->hasTemplate();
}

void TemplateTabWidget::clearTemplate()
{
    if (m_currentStrategy) {
        m_currentStrategy = m_strategies[m_currentType]; // 重新创建策略实例
        updateUIState(false);
    }
}

void TemplateTabWidget::drawTemplate()
{
    if (m_roiManager->getCurrentImage().empty()) {
        Logger::instance()->warning("请先打开图像");
        QMessageBox::warning(m_parent, "提示", "请先打开图像！");
        return;
    }
    m_view->startPolygonDrawing("template");
    //m_ui->statusbar->showMessage("请在图像上绘制模板区域（多边形）");
    Logger::instance()->info("开始绘制模板区域");
}

void TemplateTabWidget::clearTemplateDrawing()
{
    m_view->clearPolygonDrawing();
    clearMatchResults();
    //m_ui->statusbar->showMessage("已清除模板区域");
    Logger::instance()->info("已清除模板区域");
}

void TemplateTabWidget::createTemplate()
{
    if (m_roiManager->getCurrentImage().empty()) {
        QMessageBox::warning(m_parent, "提示", "请先打开图像！");
        return;
    }

    QVector<QPointF> points = m_view->getPolygonPoints();
    if (points.size() < 3) {
        QMessageBox::warning(m_parent, "提示", "请先绘制模板区域！\n需要至少 3 个顶点");
        return;
    }

    // 检查模板区域大小
    std::vector<cv::Point> cvPolygon;
    for(const QPointF& pt : points) {
        cvPolygon.push_back(cv::Point(pt.x(), pt.y()));
    }
    cv::Rect boundingRect = cv::boundingRect(cvPolygon);

    if (boundingRect.width < 20 || boundingRect.height < 20) {
        QMessageBox::warning(m_parent, "提示",
            QString("模板区域过小！\n当前大小: %1x%2 像素\n建议至少: 20x20 像素")
                .arg(boundingRect.width).arg(boundingRect.height));
        return;
    }

    bool ok;
    QString name = QInputDialog::getText(m_parent, "创建模板", "请输入模板名称:",
                                        QLineEdit::Normal, "Template_1", &ok);
    if (!ok || name.isEmpty()) return;

    createTemplateFromPolygon(points, name);

}

void TemplateTabWidget::createTemplateFromPolygon(const QVector<QPointF>& points, const QString& name)
{
    TemplateParams params = m_defaultParams;
    params.polygonPoints = points;

    Logger::instance()->info("========== 开始创建模板 ==========");
    Logger::instance()->info(QString("模板名称: %1").arg(name));
    Logger::instance()->info(QString("匹配类型: %1").arg(getCurrentStrategyName()));

    bool success = m_currentStrategy->createTemplate(m_roiManager->getCurrentImage(), points, params);

    if (success) {
        QMessageBox::information(m_parent, "成功", QString("模板创建成功！\n算法：%1").arg(getCurrentStrategyName()));
        m_view->clearPolygonDrawing();
        updateUIState(true);
        emit templateCreated(name);
    } else {
        QMessageBox::warning(m_parent, "失败", "模板创建失败，请查看日志");
    }
}

void TemplateTabWidget::findTemplate()
{
    if (!hasTemplate()) {
        QMessageBox::warning(m_parent, "提示", "请先创建模板！");
        return;
    }

    if (m_roiManager->getCurrentImage().empty()) {
        QMessageBox::warning(m_parent, "提示", "请先打开搜索图像！");
        return;
    }

    double minScore = m_ui->doubleSpinBox_minScore->value();
    int maxMatches = m_ui->spinBox_matchNumber->value();

    Logger::instance()->info("========== 开始模板匹配 ==========");
    //m_ui->statusbar->showMessage("正在搜索模板...");

    QVector<MatchResult> results = m_currentStrategy->findMatches(
        m_roiManager->getCurrentImage(), minScore, maxMatches, 0.7);

    if (results.isEmpty()) {
        Logger::instance()->info("未找到匹配目标");
        QMessageBox::information(m_parent, "结果", "未找到匹配目标");
        //m_ui->statusbar->showMessage("未找到匹配", 3000);
    } else {
        cv::Mat resultImage = m_currentStrategy->drawMatches(m_roiManager->getCurrentImage(), results);
        emit imageToShow(resultImage);

        QString msg = QString("找到 %1 个匹配目标").arg(results.size());
        //m_ui->statusbar->showMessage(msg, 5000);
        emit matchCompleted(results.size());

        QString resultText = QString("找到 %1 个匹配目标\n\n").arg(results.size());
        for (int i = 0; i < results.size(); ++i) {
            resultText += QString("#%1: %2\n").arg(i + 1).arg(results[i].toString());
        }
        QMessageBox::information(m_parent, "匹配结果", resultText);
    }
}

// void TemplateTabWidget::clearAllTemplates()
// {
//     QMessageBox::StandardButton reply = QMessageBox::question(
//         m_parent, "确认", "确定要清空所有模板吗？",
//         QMessageBox::Yes | QMessageBox::No);

//     if (reply == QMessageBox::Yes)
//     {
//         clearTemplate();
//         Logger::instance()->info("已清空所有模板");
//     }
// }

void TemplateTabWidget::clearMatchResults()
{
    if (!m_roiManager->getCurrentImage().empty()) {
        emit imageToShow(m_roiManager->getCurrentImage());
        Logger::instance()->info("已清除匹配结果");
    }
}

void TemplateTabWidget::saveTemplate()
{
    if (!hasTemplate()) {
        QMessageBox::warning(m_parent, "提示", "请先创建模板！");
        return;
    }

    // 选择保存位置
    QString imagePath = QFileDialog::getSaveFileName(m_parent, "保存模板图像", "", "PNG Images (*.png)");
    if (imagePath.isEmpty()) {
        return;
    }

    // 生成对应的JSON文件路径
    QString jsonPath = imagePath;
    jsonPath.replace(".png", ".json");

    // 调用策略保存模板
    if (m_currentStrategy->saveTemplate(imagePath, jsonPath)) {
        QMessageBox::information(m_parent, "成功", "模板保存成功！");
    } else {
        QMessageBox::warning(m_parent, "失败", "模板保存失败，请查看日志");
    }
}

void TemplateTabWidget::loadTemplate()
{
    // 选择要加载的模板图像
    QString imagePath = QFileDialog::getOpenFileName(m_parent, "选择模板图像", "", "PNG Images (*.png)");
    if (imagePath.isEmpty()) {
        return;
    }

    // 生成对应的JSON文件路径
    QString jsonPath = imagePath;
    jsonPath.replace(".png", ".json");

    // 检查JSON文件是否存在
    QFile jsonFile(jsonPath);
    if (!jsonFile.exists()) {
        QMessageBox::warning(m_parent, "提示", "找不到对应的模板参数文件！");
        return;
    }

    // 调用策略加载模板
    if (m_currentStrategy->loadTemplate(imagePath, jsonPath)) {
        updateUIState(true);
        QMessageBox::information(m_parent, "成功", "模板加载成功！");
    } else {
        QMessageBox::warning(m_parent, "失败", "模板加载失败，请查看日志");
    }
}

void TemplateTabWidget::onMatchTypeChanged(int index)
{
    Q_UNUSED(index);
    // 目前只支持 OpenCV 模板匹配
    setMatchType(MatchType::OpenCVTM);
}

void TemplateTabWidget::updateUIState(bool hasTemplate)
{
    m_ui->btn_findTemplate->setEnabled(hasTemplate);
    //m_ui->btn_clearAllTemplates->setEnabled(hasTemplate);

    if (hasTemplate) 
    {
        //m_ui->statusbar->showMessage(QString("✓ 已创建模板 [%1]").arg(getCurrentStrategyName()), 2000);
    } 
}
