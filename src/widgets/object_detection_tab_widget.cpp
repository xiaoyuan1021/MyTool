#include "widgets/object_detection_tab_widget.h"
#include "ui_object_detection_tab.h"
#include <QFileDialog>
#include <QMessageBox>
#include <QDebug>
#include <QtConcurrent/QtConcurrent>

ObjectDetectionTabWidget::ObjectDetectionTabWidget(PipelineManager* pipelineManager, QWidget* parent)
    : QWidget(parent)
    , m_ui(new Ui::ObjectDetectionTabForm)
    , m_pipelineManager(pipelineManager)
{
    m_ui->setupUi(this);
    setupConnections();
}

ObjectDetectionTabWidget::~ObjectDetectionTabWidget()
{
    delete m_ui;
}

void ObjectDetectionTabWidget::setupConnections()
{
    connect(m_ui->btn_apply, &QPushButton::clicked, this, &ObjectDetectionTabWidget::onApplyClicked);
    connect(m_ui->btn_browseModel, &QPushButton::clicked, this, &ObjectDetectionTabWidget::onBrowseModel);
    connect(m_ui->btn_browseConfig, &QPushButton::clicked, this, &ObjectDetectionTabWidget::onBrowseConfig);
    
    // 置信度阈值滑块
    connect(m_ui->slider_confidenceThreshold, &QSlider::valueChanged, this, [this](int value) {
        float threshold = value / 100.0f;
        m_ui->label_confidenceValue->setText(QString::number(threshold, 'f', 2));
    });
    
    // NMS阈值滑块
    connect(m_ui->slider_nmsThreshold, &QSlider::valueChanged, this, [this](int value) {
        float threshold = value / 100.0f;
        m_ui->label_nmsValue->setText(QString::number(threshold, 'f', 2));
    });

    // 模型加载完成信号
    connect(this, &ObjectDetectionTabWidget::modelLoadFinished, this,
        [this](bool success, const QString& message) {
            m_ui->label_status->setText(message);
            if (success) {
                qDebug() << "[ObjectDetection] model loaded, emitting detectionConfigChanged";
                emit detectionConfigChanged();
            }
        });
}

void ObjectDetectionTabWidget::onBrowseModel()
{
    QString filePath = QFileDialog::getOpenFileName(this, 
        "选择模型文件", "",
        "模型文件 (*.onnx *.prototxt *.pb *.cfg *.weights);;ONNX模型 (*.onnx);;所有文件 (*)");
    
    if (!filePath.isEmpty()) {
        m_ui->lineEdit_modelPath->setText(filePath);
    }
}

void ObjectDetectionTabWidget::onBrowseConfig()
{
    QString filePath = QFileDialog::getOpenFileName(this, 
        "选择配置文件", "",
        "配置文件 (*.prototxt *.pbtxt *.cfg);;所有文件 (*)");
    
    if (!filePath.isEmpty()) {
        m_ui->lineEdit_configPath->setText(filePath);
    }
}

void ObjectDetectionTabWidget::onApplyClicked()
{
    qDebug() << "[ObjectDetection] apply button clicked";
    m_ui->label_status->setText("状态：正在加载模型...");
    m_ui->btn_apply->setEnabled(false);
    
    // 使用QtConcurrent在后台线程加载模型，避免UI冻结
    QString modelPath = m_ui->lineEdit_modelPath->text().trimmed();
    QString configPath = m_ui->lineEdit_configPath->text().trimmed();
    
    if (modelPath.isEmpty()) {
        m_ui->label_status->setText("状态：请选择模型文件");
        m_ui->btn_apply->setEnabled(true);
        return;
    }

    QFuture<bool> future = QtConcurrent::run(
        [this, modelPath, configPath]() -> bool {
            return m_dnnInference.loadModel(modelPath, configPath);
        }
    );
    
    // 使用 QFutureWatcher 监听完成信号
    QFutureWatcher<bool>* watcher = new QFutureWatcher<bool>(this);
    connect(watcher, &QFutureWatcher<bool>::finished, this, [this, watcher, modelPath]() {
        bool success = watcher->result();
        watcher->deleteLater();
        m_ui->btn_apply->setEnabled(true);
        
        if (success) {
            emit modelLoadFinished(true, "状态：模型加载成功，等待检测");
            qDebug() << "[ObjectDetection] model loaded successfully";
        } else {
            emit modelLoadFinished(false, "状态：模型加载失败");
            qDebug() << "[ObjectDetection] model load failed";
        }
    });
    watcher->setFuture(future);
}

std::vector<DetectionResult> ObjectDetectionTabWidget::runDetection(const cv::Mat& image)
{
    if (!m_dnnInference.isLoaded() || image.empty()) {
        return {};
    }
    
    return m_dnnInference.detect(image,
        getConfidenceThreshold(),
        getNmsThreshold(),
        getInputWidth(),
        getInputHeight());
}

bool ObjectDetectionTabWidget::isModelLoaded() const
{
    return m_dnnInference.isLoaded();
}

float ObjectDetectionTabWidget::getConfidenceThreshold() const
{
    return m_ui->slider_confidenceThreshold->value() / 100.0f;
}

float ObjectDetectionTabWidget::getNmsThreshold() const
{
    return m_ui->slider_nmsThreshold->value() / 100.0f;
}

int ObjectDetectionTabWidget::getInputWidth() const
{
    return m_ui->spinBox_inputWidth->value();
}

int ObjectDetectionTabWidget::getInputHeight() const
{
    return m_ui->spinBox_inputHeight->value();
}

void ObjectDetectionTabWidget::updateDetectionResults(const std::vector<DetectionResult>& results)
{
    // 清空表格
    m_ui->tableWidget_results->setRowCount(0);
    
    if (results.empty()) {
        m_ui->label_status->setText("状态：未检测到目标");
        return;
    }
    
    // 设置表格行数
    m_ui->tableWidget_results->setRowCount(static_cast<int>(results.size()));
    
    for (int i = 0; i < static_cast<int>(results.size()); ++i) {
        const DetectionResult& det = results[i];
        
        // 类别名称
        QString className = QString::fromStdString(det.className);
        m_ui->tableWidget_results->setItem(i, 0, new QTableWidgetItem(className));
        
        // 置信度
        QString confidence = QString::number(det.confidence, 'f', 3);
        m_ui->tableWidget_results->setItem(i, 1, new QTableWidgetItem(confidence));
        
        // 位置
        QString position = QString("(%1, %2) %3x%4")
            .arg(det.box.x).arg(det.box.y)
            .arg(det.box.width).arg(det.box.height);
        m_ui->tableWidget_results->setItem(i, 2, new QTableWidgetItem(position));
    }
    
    // 更新状态
    m_ui->label_status->setText(QString("状态：检测到 %1 个目标").arg(results.size()));
}