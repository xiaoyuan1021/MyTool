#include "process_tab_widget.h"
#include "ui_process_tab.h"
#include "logger.h"
#include <QMessageBox>

ProcessTabWidget::ProcessTabWidget
(PipelineManager* pipelineManager, QWidget* parent)
    : QWidget(parent),
      m_pipelineManager(pipelineManager),
      m_ui(new Ui::Form_Process)
{
    m_ui->setupUi(this);
}

ProcessTabWidget::~ProcessTabWidget()
{
    delete m_ui;
}

void ProcessTabWidget::initialize()
{
    setupConnections();
}

void ProcessTabWidget::refreshAlgorithmListUI(const QVector<AlgorithmStep> &algorithmQueue)
{
    m_ui->algorithmListWidget->clear();
    for (const auto& step : algorithmQueue) 
    {
        QListWidgetItem *item = new QListWidgetItem(step.name);
        m_ui->algorithmListWidget->addItem(item);
    }
}

void ProcessTabWidget::setupConnections()
{
    connect(m_ui->btn_addOption, &QPushButton::clicked, this, &ProcessTabWidget::addAlgorithm);
    connect(m_ui->btn_removeOption, &QPushButton::clicked, this, &ProcessTabWidget::removeAlgorithm);
    connect(m_ui->btn_optionUp, &QPushButton::clicked, this, &ProcessTabWidget::moveAlgorithmUp);
    connect(m_ui->btn_optionDown, &QPushButton::clicked, this, &ProcessTabWidget::moveAlgorithmDown);
    connect(m_ui->comboBox_selectAlgorithm, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &ProcessTabWidget::onAlgorithmTypeChanged);
    connect(m_ui->algorithmListWidget, &QListWidget::currentItemChanged,
            this, &ProcessTabWidget::onAlgorithmSelectionChanged);
}

void ProcessTabWidget::addAlgorithm()
{
    saveCurrentEdit();

    AlgorithmStep step;
    int index = m_ui->comboBox_selectAlgorithm->currentIndex();
    step.name = m_ui->comboBox_selectAlgorithm->currentText();
    step.type = "OpenCVAlgorithm";
    step.enabled = true;
    step.description = "OpenCV图像处理算法";
    step.params["OpenCVAlgoType"] = index;

    switch(index) {
    case 0: case 2: case 4: case 6:
        step.params["radius"] = m_ui->doubleSpinBox_radius->value();
        break;
    case 1: case 3: case 5: case 7:
        step.params["width"] = m_ui->spinBox_width->value();
        step.params["height"] = m_ui->spinBox_height->value();
        break;
    case 11:
        step.params["shapeType"] = m_ui->comboBox_shapeType->currentData().toString();
        break;
    }

    m_pipelineManager->addAlgorithmStep(step);
    QListWidgetItem *item = new QListWidgetItem(step.name);
    m_ui->algorithmListWidget->addItem(item);

    Logger::instance()->info(QString("添加算法 %1").arg(step.name));
    emit algorithmChanged();
}

void ProcessTabWidget::removeAlgorithm()
{
    saveCurrentEdit();
    int row = m_ui->algorithmListWidget->currentRow();
    if (row < 0) return;

    m_pipelineManager->removeAlgorithmStep(row);
    delete m_ui->algorithmListWidget->takeItem(row);

    Logger::instance()->info("移除算法");
    emit algorithmChanged();
}

void ProcessTabWidget::moveAlgorithmUp()
{
    saveCurrentEdit();
    int currentRow = m_ui->algorithmListWidget->currentRow();

    if (currentRow <= 0) return;

    m_pipelineManager->swapAlgorithmStep(currentRow, currentRow - 1);

    QListWidgetItem *item = m_ui->algorithmListWidget->takeItem(currentRow);
    m_ui->algorithmListWidget->insertItem(currentRow - 1, item);
    m_ui->algorithmListWidget->setCurrentRow(currentRow - 1);

    emit algorithmChanged();
    //m_ui->statusbar->showMessage("算法步骤已上移", 1000);
}

void ProcessTabWidget::moveAlgorithmDown()
{
    saveCurrentEdit();
    int currentRow = m_ui->algorithmListWidget->currentRow();

    if (currentRow < 0 || currentRow >= m_ui->algorithmListWidget->count() - 1) return;

    m_pipelineManager->swapAlgorithmStep(currentRow, currentRow + 1);

    QListWidgetItem *item = m_ui->algorithmListWidget->takeItem(currentRow);
    m_ui->algorithmListWidget->insertItem(currentRow + 1, item);
    m_ui->algorithmListWidget->setCurrentRow(currentRow + 1);

    emit algorithmChanged();
    //m_ui->statusbar->showMessage("算法步骤已下移", 1000);
}

void ProcessTabWidget::onAlgorithmTypeChanged(int index)
{
    switch (index) {
    case 0: case 2: case 4: case 6:
        m_ui->stackedWidget_Algorithm->setCurrentIndex(0);
        break;
    case 1: case 3: case 5: case 7:
        m_ui->stackedWidget_Algorithm->setCurrentIndex(1);
        break;
    case 8: case 9: case 10:
        m_ui->stackedWidget_Algorithm->setCurrentIndex(2);
        break;
    case 11:
        m_ui->stackedWidget_Algorithm->setCurrentIndex(3);
        break;
    }
}

void ProcessTabWidget::onAlgorithmSelectionChanged(QListWidgetItem* current, QListWidgetItem* previous)
{
    Q_UNUSED(previous);

    if (m_editingAlgorithmIndex >= 0) {
        saveCurrentEdit();
    }

    if (current) {
        int newIndex = m_ui->algorithmListWidget->row(current);
        loadAlgorithmParameters(newIndex);
        m_editingAlgorithmIndex = newIndex;

        // m_ui->statusbar->showMessage(
        //     QString("正在编辑: %1 (修改参数后点击其他项自动保存)")
        //         .arg(current->text()), 3000);
    } else {
        m_editingAlgorithmIndex = -1;
    }
}

void ProcessTabWidget::saveCurrentEdit()
{
    if (m_editingAlgorithmIndex < 0) return;

    const QVector<AlgorithmStep>& queue = m_pipelineManager->getAlgorithmQueue();
    if (m_editingAlgorithmIndex >= queue.size()) {
        m_editingAlgorithmIndex = -1;
        return;
    }

    AlgorithmStep step = queue[m_editingAlgorithmIndex];
    int algoType = step.params["OpenCVAlgoType"].toInt();

    switch(algoType) {
    case 0: case 2: case 4: case 6:
        step.params["radius"] = m_ui->doubleSpinBox_radius->value();
        break;
    case 1: case 3: case 5: case 7:
        step.params["width"] = m_ui->spinBox_width->value();
        step.params["height"] = m_ui->spinBox_height->value();
        break;
    case 11:
        step.params["shapeType"] = m_ui->comboBox_shapeType->currentData().toString();
        break;
    }

    m_pipelineManager->updateAlgorithmStep(m_editingAlgorithmIndex, step);
    emit algorithmChanged();

    // Logger::instance()->info(QString("已保存算法 #%1: %2 的参数修改")
    //     .arg(m_editingAlgorithmIndex + 1).arg(step.name));
}

void ProcessTabWidget::loadAlgorithmParameters(int index)
{
    const QVector<AlgorithmStep>& queue = m_pipelineManager->getAlgorithmQueue();
    if (index < 0 || index >= queue.size()) return;

    const AlgorithmStep& step = queue[index];
    int algoType = step.params["OpenCVAlgoType"].toInt();

    switch(algoType) {
    case 0: case 2: case 4: case 6:
        m_ui->stackedWidget_Algorithm->setCurrentIndex(0);
        m_ui->doubleSpinBox_radius->setValue(step.params.value("radius", 3.5).toDouble());
        break;
    case 1: case 3: case 5: case 7:
        m_ui->stackedWidget_Algorithm->setCurrentIndex(1);
        m_ui->spinBox_width->setValue(step.params.value("width", 5).toInt());
        m_ui->spinBox_height->setValue(step.params.value("height", 5).toInt());
        break;
    case 8: case 9: case 10:
        m_ui->stackedWidget_Algorithm->setCurrentIndex(2);
        break;
    case 11:
        m_ui->stackedWidget_Algorithm->setCurrentIndex(3);
        QString shapeType = step.params.value("shapeType", "convex").toString();
        int comboIndex = m_ui->comboBox_shapeType->findData(shapeType);
        if (comboIndex >= 0) {
            m_ui->comboBox_shapeType->setCurrentIndex(comboIndex);
        }
        break;
    }
}