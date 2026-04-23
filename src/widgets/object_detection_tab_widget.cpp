#include "widgets/object_detection_tab_widget.h"
#include "ui_object_detection_tab.h"
#include <QFileDialog>
#include <QMessageBox>
#include <QDebug>

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
    qDebug() << "[ObjectDetection] 应用按钮被点击";
    updateConfig();
}

void ObjectDetectionTabWidget::updateConfig()
{
    QString modelPath = m_ui->lineEdit_modelPath->text().trimmed();
    QString configPath = m_ui->lineEdit_configPath->text().trimmed();
    float confidenceThreshold = m_ui->slider_confidenceThreshold->value() / 100.0f;
    float nmsThreshold = m_ui->slider_nmsThreshold->value() / 100.0f;
    int inputWidth = m_ui->spinBox_inputWidth->value();
    int inputHeight = m_ui->spinBox_inputHeight->value();

    qDebug() << "[ObjectDetection] updateConfig:"
             << "模型路径:" << modelPath
             << "配置路径:" << configPath
             << "置信度阈值:" << confidenceThreshold
             << "NMS阈值:" << nmsThreshold
             << "输入尺寸:" << inputWidth << "x" << inputHeight;

    // 校验模型路径
    if (modelPath.isEmpty()) {
        qDebug() << "[ObjectDetection] 模型路径为空，跳过加载";
        m_ui->label_status->setText("状态：请选择模型文件");
        return;
    }

    // 尝试加载模型
    qDebug() << "[ObjectDetection] 正在加载模型...";
    bool success = m_dnnInference.loadModel(modelPath, configPath);
    if (success) {
        qDebug() << "[ObjectDetection] 模型加载成功";
        m_ui->label_status->setText("状态：模型加载成功，等待检测");
        
        // 发射配置变更信号
        emit detectionConfigChanged();
    } else {
        qDebug() << "[ObjectDetection] 模型加载失败";
        m_ui->label_status->setText("状态：模型加载失败");
    }
}

void ObjectDetectionTabWidget::updateDetectionResults(const cv::Mat& image, 
                                                       const std::vector<cv::Rect>& boxes, 
                                                       const std::vector<float>& confidences, 
                                                       const std::vector<int>& classIds,
                                                       const std::vector<std::string>& classNames)
{
    // 清空表格
    m_ui->tableWidget_results->setRowCount(0);
    
    if (boxes.empty()) {
        m_ui->label_status->setText("状态：未检测到目标");
        return;
    }
    
    // 设置表格行数
    m_ui->tableWidget_results->setRowCount(static_cast<int>(boxes.size()));
    
    for (int i = 0; i < static_cast<int>(boxes.size()); ++i) {
        // 类别名称
        QString className = (i < static_cast<int>(classNames.size())) 
            ? QString::fromStdString(classNames[i]) 
            : QString::number(classIds[i]);
        m_ui->tableWidget_results->setItem(i, 0, new QTableWidgetItem(className));
        
        // 置信度
        QString confidence = QString::number(confidences[i], 'f', 3);
        m_ui->tableWidget_results->setItem(i, 1, new QTableWidgetItem(confidence));
        
        // 位置
        QString position = QString("(%1, %2) %3×%4")
            .arg(boxes[i].x).arg(boxes[i].y)
            .arg(boxes[i].width).arg(boxes[i].height);
        m_ui->tableWidget_results->setItem(i, 2, new QTableWidgetItem(position));
    }
    
    // 更新状态
    m_ui->label_status->setText(QString("状态：检测到 %1 个目标").arg(boxes.size()));
}