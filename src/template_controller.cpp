#include "template_controller.h"
#include "ui_mainwindow.h"
#include "image_view.h"
#include "logger.h"
#include <QMessageBox>
#include <QInputDialog>

TemplateController::TemplateController(Ui::MainWindow* ui,
                                     ImageView* view,
                                     RoiManager* roiManager,
                                     QWidget* parent)
    : QObject(parent)
    , m_ui(ui)
    , m_view(view)
    , m_roiManager(roiManager)
    , m_parent(parent)
    , m_currentType(MatchType::ShapeModel)
{
    // 初始化默认参数
    m_defaultParams.numLevels = 0;
    m_defaultParams.angleStart = -10.0;
    m_defaultParams.angleExtent = 20.0;
    m_defaultParams.angleStep = 1.0;
    m_defaultParams.optimization = "auto";
    m_defaultParams.metric = "use_polarity";
    m_defaultParams.nccLevels = 0;
    m_defaultParams.matchMethod = cv::TM_CCOEFF_NORMED;
}

void TemplateController::initialize()
{
    initializeStrategies();

    // 连接信号槽
    connect(m_ui->btn_drawTemplate, &QPushButton::clicked, this, &TemplateController::drawTemplate);
    connect(m_ui->btn_clearTemplate, &QPushButton::clicked, this, &TemplateController::clearTemplateDrawing);
    connect(m_ui->btn_creatTemplate, &QPushButton::clicked, this, &TemplateController::createTemplate);
    connect(m_ui->btn_findTemplate, &QPushButton::clicked, this, &TemplateController::findTemplate);
    connect(m_ui->btn_clearAllTemplates, &QPushButton::clicked, this, &TemplateController::clearAllTemplates);
    connect(m_ui->comboBox_matchType, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &TemplateController::onMatchTypeChanged);
}

void TemplateController::initializeStrategies()
{
    m_strategies[MatchType::ShapeModel] = std::make_shared<ShapeMatchStrategy>();
    m_strategies[MatchType::NCCModel] = std::make_shared<NCCMatchStrategy>();
    m_strategies[MatchType::OpenCVTM] = std::make_shared<OpenCVMatchStrategy>();
    m_currentStrategy = m_strategies[m_currentType];
}

void TemplateController::setMatchType(MatchType type)
{
    if (m_currentType != type) {
        m_currentType = type;
        m_currentStrategy = m_strategies[type];
    }
}

QString TemplateController::getCurrentStrategyName() const
{
    return m_currentStrategy ? m_currentStrategy->getStrategyName() : "Unknown";
}

bool TemplateController::hasTemplate() const
{
    return m_currentStrategy && m_currentStrategy->hasTemplate();
}

void TemplateController::clearTemplate()
{
    if (m_currentStrategy) {
        m_currentStrategy = m_strategies[m_currentType]; // 重新创建策略实例
        updateUIState(false);
    }
}

void TemplateController::drawTemplate()
{
    if (m_roiManager->getCurrentImage().empty()) {
        Logger::instance()->warning("请先打开图像");
        QMessageBox::warning(m_parent, "提示", "请先打开图像！");
        return;
    }
    m_view->startPolygonDrawing("template");
    m_ui->statusbar->showMessage("请在图像上绘制模板区域（多边形）");
    Logger::instance()->info("开始绘制模板区域");
}

void TemplateController::clearTemplateDrawing()
{
    m_view->clearPolygonDrawing();
    m_ui->statusbar->showMessage("已清除模板区域");
    Logger::instance()->info("已清除模板区域");
}

void TemplateController::createTemplate()
{
    if (m_roiManager->getCurrentImage().empty()) {
        QMessageBox::warning(m_parent, "提示", "请先打开图像！");
        return;
    }

    QVector<QPointF> points = m_view->getPolygonPoints();
    if (points.size() < 3) {
        QMessageBox::warning(m_parent, "提示", "请先绘制模板区域！");
        return;
    }

    bool ok;
    QString name = QInputDialog::getText(m_parent, "创建模板", "请输入模板名称:",
                                        QLineEdit::Normal, "Template_1", &ok);
    if (!ok || name.isEmpty()) return;

    createTemplateFromPolygon(points, name);
}

void TemplateController::createTemplateFromPolygon(const QVector<QPointF>& points, const QString& name)
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
        m_ui->statusbar->showMessage("模板创建成功", 3000);
        updateUIState(true);
        emit templateCreated(name);
    } else {
        QMessageBox::warning(m_parent, "失败", "模板创建失败，请查看日志");
        m_ui->statusbar->showMessage("模板创建失败", 3000);
    }
}

void TemplateController::findTemplate()
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
    m_ui->statusbar->showMessage("正在搜索模板...");

    QVector<MatchResult> results = m_currentStrategy->findMatches(
        m_roiManager->getCurrentImage(), minScore, maxMatches, 0.75);

    if (results.isEmpty()) {
        Logger::instance()->info("未找到匹配目标");
        QMessageBox::information(m_parent, "结果", "未找到匹配目标");
        m_ui->statusbar->showMessage("未找到匹配", 3000);
    } else {
        cv::Mat resultImage = m_currentStrategy->drawMatches(m_roiManager->getCurrentImage(), results);
        emit imageToShow(resultImage);

        QString msg = QString("找到 %1 个匹配目标").arg(results.size());
        m_ui->statusbar->showMessage(msg, 5000);
        emit matchCompleted(results.size());

        QString resultText = QString("找到 %1 个匹配目标\n\n").arg(results.size());
        for (int i = 0; i < results.size(); ++i) {
            resultText += QString("#%1: %2\n").arg(i + 1).arg(results[i].toString());
        }
        QMessageBox::information(m_parent, "匹配结果", resultText);
    }
}

void TemplateController::clearAllTemplates()
{
    QMessageBox::StandardButton reply = QMessageBox::question(
        m_parent, "确认", "确定要清空所有模板吗？",
        QMessageBox::Yes | QMessageBox::No);

    if (reply == QMessageBox::Yes) 
    {
        clearTemplate();
        Logger::instance()->info("已清空所有模板");
        m_ui->statusbar->showMessage("已清空所有模板", 3000);
    }
}

void TemplateController::onMatchTypeChanged(int index)
{
    Q_UNUSED(index);
    QString typeName = m_ui->comboBox_matchType->currentText();
    MatchType type = (typeName == "NCC Model") ? MatchType::NCCModel :
                     (typeName == "Opencv Model") ? MatchType::OpenCVTM : MatchType::ShapeModel;

    setMatchType(type);
    m_ui->statusbar->showMessage(QString("当前匹配算法: %1").arg(typeName), 2000);
}

void TemplateController::updateUIState(bool hasTemplate)
{
    m_ui->btn_findTemplate->setEnabled(hasTemplate);
    m_ui->btn_clearAllTemplates->setEnabled(hasTemplate);

    if (hasTemplate) 
    {
        m_ui->statusbar->showMessage(QString("✓ 已创建模板 [%1]").arg(getCurrentStrategyName()), 2000);
    }
}