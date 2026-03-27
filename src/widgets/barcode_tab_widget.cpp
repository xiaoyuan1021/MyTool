#include "widgets/barcode_tab_widget.h"
#include "ui_barcode_tab.h"
#include <QDebug>

BarcodeTabWidget::BarcodeTabWidget(PipelineManager* pipelineManager,
                                   std::function<void()> processCallback,
                                   QWidget* parent)
    : QWidget(parent)
    , m_pipelineManager(pipelineManager)
    , m_processCallback(processCallback)
    , m_ui(new Ui::BarcodeTabForm)
{
    m_ui->setupUi(this);
    
    // 连接信号和槽
    connect(m_ui->chk_enableBarcode, &QCheckBox::toggled,
            this, &BarcodeTabWidget::onEnableBarcodeChanged);
    connect(m_ui->comboBox_codeType, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &BarcodeTabWidget::onCodeTypeChanged);
    connect(m_ui->spinBox_maxNum, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &BarcodeTabWidget::onMaxNumChanged);
    connect(m_ui->btn_apply, &QPushButton::clicked,
            this, &BarcodeTabWidget::handleApply);
    
    // 初始化表格
    m_ui->tableWidget_results->setHorizontalHeaderLabels(
        QStringList() << "条码类型" << "数据" << "质量" << "位置");
    m_ui->tableWidget_results->horizontalHeader()->setStretchLastSection(true);
    
    // 初始禁用设置
    m_ui->groupBox_settings->setEnabled(false);
}

BarcodeTabWidget::~BarcodeTabWidget()
{
    delete m_ui;
}

BarcodeConfig BarcodeTabWidget::getBarcodeConfig() const
{
    BarcodeConfig config;
    config.enableBarcode = m_ui->chk_enableBarcode->isChecked();
    
    // 获取条码类型
    QString codeType = m_ui->comboBox_codeType->currentText();
    if (codeType == "自动检测")
    {
        config.codeTypes = QStringList() << "auto";
    }
    else
    {
        config.codeTypes = QStringList() << codeType;
    }
    
    config.maxNumSymbols = m_ui->spinBox_maxNum->value();
    
    return config;
}

void BarcodeTabWidget::updatePipelineConfig()
{
    if (!m_pipelineManager) return;
    
    PipelineConfig& config = m_pipelineManager->getConfig();
    config.barcode = getBarcodeConfig();
    
    // 如果禁用了条码识别，清空结果
    if (!config.barcode.enableBarcode)
    {
        m_ui->tableWidget_results->setRowCount(0);
        m_ui->label_status->setText("状态：未识别");
    }
}

void BarcodeTabWidget::updateResultsTable(const QVector<BarcodeResult>& results)
{
    m_ui->tableWidget_results->setRowCount(results.size());
    
    for (int i = 0; i < results.size(); i++)
    {
        const BarcodeResult& result = results[i];
        
        // 条码类型
        QTableWidgetItem* typeItem = new QTableWidgetItem(result.type);
        m_ui->tableWidget_results->setItem(i, 0, typeItem);
        
        // 数据
        QTableWidgetItem* dataItem = new QTableWidgetItem(result.data);
        m_ui->tableWidget_results->setItem(i, 1, dataItem);
        
        // 质量
        QString qualityText;
        if (result.quality >= 0)
        {
            qualityText = QString::number(result.quality, 'f', 1) + "%";
        }
        else
        {
            qualityText = "N/A";
        }
        QTableWidgetItem* qualityItem = new QTableWidgetItem(qualityText);
        m_ui->tableWidget_results->setItem(i, 2, qualityItem);
        
        // 位置
        QString locationText = QString("(%1, %2) %3x%4")
            .arg(result.location.x(), 0, 'f', 0)
            .arg(result.location.y(), 0, 'f', 0)
            .arg(result.location.width(), 0, 'f', 0)
            .arg(result.location.height(), 0, 'f', 0);
        QTableWidgetItem* locationItem = new QTableWidgetItem(locationText);
        m_ui->tableWidget_results->setItem(i, 3, locationItem);
    }
}

void BarcodeTabWidget::updateStatus(const QString& status)
{
    m_ui->label_status->setText("状态：" + status);
}

void BarcodeTabWidget::onEnableBarcodeChanged(bool enabled)
{
    m_ui->groupBox_settings->setEnabled(enabled);
    updatePipelineConfig();
}

void BarcodeTabWidget::onCodeTypeChanged(int index)
{
    Q_UNUSED(index);
    updatePipelineConfig();
}

void BarcodeTabWidget::onMaxNumChanged(int value)
{
    Q_UNUSED(value);
    updatePipelineConfig();
}

void BarcodeTabWidget::onReturnQualityChanged(bool checked)
{
    Q_UNUSED(checked);
    updatePipelineConfig();
}

void BarcodeTabWidget::handleApply()
{
    updatePipelineConfig();
    
    if (m_processCallback)
    {
        m_processCallback();
    }
    
    //qDebug() << "[BarcodeTab] 应用条码识别设置";
}