#include "template_ui_controller.h"
#include "ui_mainwindow.h"
#include "image_view.h"
#include "logger.h"
#include <QMessageBox>
#include <QInputDialog>

TemplateUIController::TemplateUIController(Ui::MainWindow* ui,
                                         TemplateMatchManager* templateManager,
                                         ImageView* view,
                                         RoiManager* roiManager,
                                         QWidget* parent)
    : QObject(parent)
    , m_ui(ui)
    , m_templateManager(templateManager)
    , m_view(view)
    , m_roiManager(roiManager)
    , m_parent(parent)
{
}

void TemplateUIController::initialize()
{
    // 连接信号槽
    connect(m_ui->btn_drawTemplate, &QPushButton::clicked, this, &TemplateUIController::drawTemplate);
    connect(m_ui->btn_clearTemplate, &QPushButton::clicked, this, &TemplateUIController::clearTemplate);
    connect(m_ui->btn_creatTemplate, &QPushButton::clicked, this, &TemplateUIController::createTemplate);
    connect(m_ui->btn_findTemplate, &QPushButton::clicked, this, &TemplateUIController::findTemplate);
    connect(m_ui->btn_clearAllTemplates, &QPushButton::clicked, this, &TemplateUIController::clearAllTemplates);
    connect(m_ui->comboBox_matchType, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &TemplateUIController::onMatchTypeChanged);
}

void TemplateUIController::drawTemplate()
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

void TemplateUIController::clearTemplate()
{
    m_view->clearPolygonDrawing();
    m_ui->statusbar->showMessage("已清除模板区域");
    Logger::instance()->info("已清除模板区域");
}

void TemplateUIController::createTemplate()
{
    // 检查是否有图像
    if (m_roiManager->getCurrentImage().empty()) {
        QMessageBox::warning(m_parent, "提示", "请先打开图像！");
        return;
    }

    // 检查是否绘制了多边形
    QVector<QPointF> points = m_view->getPolygonPoints();
    if (points.size() < 3) {
        QMessageBox::warning(m_parent, "提示", "请先绘制模板区域！");
        return;
    }

    // 输入模板名称
    bool ok;
    QString name = QInputDialog::getText(m_parent, "创建模板", "请输入模板名称:",
                                        QLineEdit::Normal, "Template_1", &ok);
    if (!ok || name.isEmpty()) return;

    createTemplateFromPolygon(points, name);
}

void TemplateUIController::createTemplateFromPolygon(const QVector<QPointF>& points, const QString& name)
{
    // 准备参数
    TemplateParams params = m_templateManager->getDefaultParams();
    params.polygonPoints = points;

    // 根据当前匹配类型设置参数
    MatchType currentType = m_templateManager->getCurrentMatchType();
    switch (currentType) {
    case MatchType::ShapeModel: break;
    case MatchType::NCCModel: params.nccLevels = 0; break;
    case MatchType::OpenCVTM: params.matchMethod = cv::TM_CCOEFF_NORMED; break;
    }

    // 创建模板
    Logger::instance()->info("========== 开始创建模板 ==========");
    Logger::instance()->info(QString("模板名称: %1").arg(name));
    Logger::instance()->info(QString("匹配类型: %1").arg(m_templateManager->getCurrentStrategyName()));

    bool success = m_templateManager->createTemplate(name, m_roiManager->getCurrentImage(), points, params);

    if (success) {
        QMessageBox::information(m_parent, "成功",
            QString("模板创建成功！\n算法：%1").arg(m_templateManager->getCurrentStrategyName()));
        m_view->clearPolygonDrawing();
        m_ui->statusbar->showMessage("模板创建成功", 3000);
        updateUIState(true);
    } else {
        QMessageBox::warning(m_parent, "失败", "模板创建失败，请查看日志");
        m_ui->statusbar->showMessage("模板创建失败", 3000);
    }
}

void TemplateUIController::findTemplate()
{
    // 检查是否创建了模板
    if (!m_templateManager->hasTemplate()) {
        QMessageBox::warning(m_parent, "提示", "请先创建模板！");
        return;
    }

    // 检查是否有搜索图像
    if (m_roiManager->getCurrentImage().empty()) {
        QMessageBox::warning(m_parent, "提示", "请先打开搜索图像！");
        return;
    }

    // 从UI获取参数
    double minScore = m_ui->doubleSpinBox_minScore->value();
    int maxMatches = m_ui->spinBox_matchNumber->value();

    // 执行匹配
    Logger::instance()->info("========== 开始模板匹配 ==========");
    m_ui->statusbar->showMessage("正在搜索模板...");

    QVector<MatchResult> results = m_templateManager->findTemplate(
        m_roiManager->getCurrentImage(), minScore, maxMatches, 0.75);

    // 显示结果
    if (results.isEmpty()) {
        Logger::instance()->info("未找到匹配目标");
        QMessageBox::information(m_parent, "结果", "未找到匹配目标");
        m_ui->statusbar->showMessage("未找到匹配", 3000);
    } else {
        // 绘制匹配结果
        cv::Mat resultImage = m_templateManager->drawMatches(m_roiManager->getCurrentImage(), results);

        // 通过信号通知主窗口显示图像
        emit imageToShow(resultImage);

        QString msg = QString("找到 %1 个匹配目标").arg(results.size());
        m_ui->statusbar->showMessage(msg, 5000);

        QString resultText = QString("找到 %1 个匹配目标\n\n").arg(results.size());
        for (int i = 0; i < results.size(); ++i) {
            resultText += QString("#%1: %2\n").arg(i + 1).arg(results[i].toString());
        }
        QMessageBox::information(m_parent, "匹配结果", resultText);
    }
}

void TemplateUIController::clearAllTemplates()
{
    QMessageBox::StandardButton reply = QMessageBox::question(
        m_parent, "确认", "确定要清空所有模板吗？",
        QMessageBox::Yes | QMessageBox::No);

    if (reply == QMessageBox::Yes) {
        m_templateManager->clearTemplate();
        Logger::instance()->info("已清空所有模板");
        m_ui->statusbar->showMessage("已清空所有模板", 3000);
        updateUIState(false);
    }
}

void TemplateUIController::onMatchTypeChanged(int index)
{
    Q_UNUSED(index);
    QString typeName = m_ui->comboBox_matchType->currentText();
    MatchType type;

    if (typeName == "ShapeModel") {
        type = MatchType::ShapeModel;
    } else if (typeName == "NCC Model") {
        type = MatchType::NCCModel;
    } else if (typeName == "Opencv Model") {
        type = MatchType::OpenCVTM;
    } else {
        type = MatchType::ShapeModel;
    }

    m_templateManager->setMatchType(type);
    m_ui->statusbar->showMessage(QString("当前匹配算法: %1").arg(typeName), 2000);
}

void TemplateUIController::updateUIState(bool hasTemplate)
{
    m_ui->btn_findTemplate->setEnabled(hasTemplate);
    m_ui->btn_clearAllTemplates->setEnabled(hasTemplate);

    if (hasTemplate) {
        m_ui->statusbar->showMessage(
            QString("✓ 已创建模板 [%1]").arg(m_templateManager->getCurrentStrategyName()), 2000);
    }
}