#include "judge_tab_widget.h"
#include "ui_judge_tab.h"
#include <QMessageBox>

JudgeTabWidget::JudgeTabWidget(PipelineManager* pipelineManager, QWidget *parent)
    : QWidget(parent)
    , m_ui(new Ui::Form_Judge)
    , m_pipelineManager(pipelineManager)
{
    m_ui->setupUi(this);
    m_ui->lineEdit_nowRegions->setReadOnly(true);
    connect(m_ui->btn_runTest, &QPushButton::clicked, this, &JudgeTabWidget::onRunTestClicked);
}

JudgeTabWidget::~JudgeTabWidget()
{
    delete m_ui;
}

void JudgeTabWidget::setCurrentRegionCount(int count)
{
    count = m_pipelineManager->getLastContext().currentRegions;
    m_ui->lineEdit_nowRegions->setText(QString::number(count));
}

void JudgeTabWidget::onRunTestClicked()
{
    bool ok1, ok2;
    int minRegions = m_ui->lineEdit_minRegionCount->text().toInt(&ok1);
    int maxRegions = m_ui->lineEdit_maxRegionCount->text().toInt(&ok2);

    if (!ok1 || !ok2) {
        QMessageBox::warning(this, "输入错误", "请输入有效的数字！");
        return;
    }

    if (minRegions < 0 || maxRegions < minRegions) {
        QMessageBox::warning(this, "输入错误",
                           "最小值不能为负数，且最大值不能小于最小值！");
        return;
    }

    int currentCount = m_pipelineManager->getLastContext().currentRegions;
    bool pass = (currentCount >= minRegions && currentCount <= maxRegions);

    if (pass) 
    {
        QMessageBox::information(this, "判定结果",
                               QString("✅ OK\n当前区域数: %1\n符合范围: [%2, %3]")
                                   .arg(currentCount).arg(minRegions).arg(maxRegions));
    } 
    else 
    {
        QMessageBox::warning(this, "判定结果",
                           QString("❌ NG\n当前区域数: %1\n要求范围: [%2, %3]")
                               .arg(currentCount).arg(minRegions).arg(maxRegions));
    }
}

void JudgeTabWidget::getJudgeConfig(int& minCount, int& maxCount, int& currentCount) const
{
    minCount = m_ui->lineEdit_minRegionCount->text().toInt();
    maxCount = m_ui->lineEdit_maxRegionCount->text().toInt();
    currentCount = m_ui->lineEdit_nowRegions->text().toInt();
}

void JudgeTabWidget::setJudgeConfig(int minCount, int maxCount, int currentCount)
{
    m_ui->lineEdit_minRegionCount->setText(QString::number(minCount));
    m_ui->lineEdit_maxRegionCount->setText(QString::number(maxCount));
    m_ui->lineEdit_nowRegions->setText(QString::number(currentCount));
}
