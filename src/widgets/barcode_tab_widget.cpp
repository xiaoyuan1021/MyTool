#include "widgets/barcode_tab_widget.h"
#include "ui_barcode_tab.h"
#include "config/config_manager.h"
#include "controllers/roi_ui_controller.h"
#include <QDebug>
#include <QSignalBlocker>

BarcodeTabWidget::BarcodeTabWidget(PipelineManager* pipelineManager,
                                   QWidget* parent)
    : QWidget(parent)
    , m_pipelineManager(pipelineManager)
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

void BarcodeTabWidget::setBarcodeConfig(const BarcodeConfig& config)
{
    // 阻止信号，避免不完整的中间状态写入 PipelineManager
    QSignalBlocker blocker1(m_ui->chk_enableBarcode);
    QSignalBlocker blocker2(m_ui->comboBox_codeType);
    QSignalBlocker blocker3(m_ui->spinBox_maxNum);

    m_ui->chk_enableBarcode->setChecked(config.enableBarcode);

    if (!config.codeTypes.isEmpty()) {
        QString codeType = config.codeTypes.first();
        if (codeType == "auto") {
            m_ui->comboBox_codeType->setCurrentIndex(0);
        } else {
            int idx = m_ui->comboBox_codeType->findText(codeType);
            if (idx >= 0) {
                m_ui->comboBox_codeType->setCurrentIndex(idx);
            }
        }
    }

    
    m_ui->spinBox_maxNum->setValue(config.maxNumSymbols);

    // 手动更新 groupBox 和 Pipeline（使用完整的新配置）
    m_ui->groupBox_settings->setEnabled(config.enableBarcode);
    updatePipelineConfig();
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
    
    PipelineConfig config = m_pipelineManager->getConfigSnapshot();
    config.barcode = getBarcodeConfig();
    m_pipelineManager->setConfig(config);

    emit barcodeConfigChanged();
    
    // 如果禁用了条码识别，清空结果
    if (!config.barcode.enableBarcode)
    {
        m_ui->tableWidget_results->setRowCount(0);
        m_ui->label_status->setText("状态：未识别");
    }
}

void BarcodeTabWidget::updateFromPipeline(const PipelineContext& ctx)
{
    updateResultsTable(ctx.barcodeResults);
    updateStatus(ctx.barcodeStatus);
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
    // 只更新本地配置，不执行pipeline，等待用户点击"应用"按钮
}

void BarcodeTabWidget::onCodeTypeChanged(int index)
{
    Q_UNUSED(index);
    // 只更新本地配置，不执行pipeline
}

void BarcodeTabWidget::onMaxNumChanged(int value)
{
    Q_UNUSED(value);
    // 只更新本地配置，不执行pipeline
}

void BarcodeTabWidget::onReturnQualityChanged(bool checked)
{
    Q_UNUSED(checked);
    // 只更新本地配置，不执行pipeline
}

void BarcodeTabWidget::handleApply()
{
    // 用户点击"应用"按钮时才更新配置并执行pipeline
    updatePipelineConfig();
    emit requestApplyBarcodeSettings();
}

void BarcodeTabWidget::connectSignals(const SignalContext& ctx,
                                       std::function<void()> onExecutePipeline,
                                       std::function<void()> onConfigSaved)
{
    Q_UNUSED(onConfigSaved);

    // ★ 条码配置变化时同步到DetectionItem.config
    auto syncToDetectionItem = [this, ctx]() {
        if (ctx.roiCtrl) {
            ctx.roiCtrl->updateBarcodeDetectionConfig(getBarcodeConfig());
        }
    };

    connect(m_ui->chk_enableBarcode, &QCheckBox::toggled, this, syncToDetectionItem);
    connect(m_ui->comboBox_codeType, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this, syncToDetectionItem](int) { syncToDetectionItem(); });
    connect(m_ui->spinBox_maxNum, QOverload<int>::of(&QSpinBox::valueChanged),
            this, [this, syncToDetectionItem](int) { syncToDetectionItem(); });

    // 条码识别需要用户点击"应用"按钮才执行，不使用自动预览
    connect(this, &BarcodeTabWidget::requestApplyBarcodeSettings,
            this, onExecutePipeline);
}

// ========== IConfigurableTab 接口实现 ==========

void BarcodeTabWidget::saveToConfig(PipelineConfig& config) const
{
    config.barcode = getBarcodeConfig();
}

void BarcodeTabWidget::loadFromConfig(const PipelineConfig& config)
{
    setBarcodeConfig(config.barcode);
}

// ========== ITabInitializable 接口实现 ==========

void BarcodeTabWidget::initializeTab(const TabInitContext& ctx)
{
    if (ctx.pipelineManager) {
        PipelineConfig pc = ctx.pipelineManager->getConfigSnapshot();
        setBarcodeConfig(pc.barcode);
    }
}
