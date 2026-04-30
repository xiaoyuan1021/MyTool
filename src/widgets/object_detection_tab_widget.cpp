#include "widgets/object_detection_tab_widget.h"
#include "ui_object_detection_tab.h"
#include "ui/logger.h"
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

    QString modelPath = m_ui->lineEdit_modelPath->text().trimmed();
    if (modelPath.isEmpty()) {
        m_ui->label_status->setText("状态：请选择模型文件");
        return;
    }

    // 如果模型已加载且路径相同，无需重新加载
    if ((m_ortInference.isLoaded() || m_dnnInference.isLoaded()) && m_currentModelPath == modelPath) {
        qDebug() << "[ObjectDetection] model already loaded, skip reloading";
        m_ui->label_status->setText("状态：模型已就绪");
        emit detectionConfigChanged();
        return;
    }

    m_ui->label_status->setText("状态：正在加载模型...");
    m_ui->btn_apply->setEnabled(false);

    QString configPath = m_ui->lineEdit_configPath->text().trimmed();

    QFuture<QString> future = QtConcurrent::run(
        [this, modelPath, configPath]() -> QString {
            // 优先尝试 ONNX Runtime GPU
            if (m_ortInference.loadModel(modelPath, true)) {
                spdlog::info("[ObjectDetection] ONNX Runtime GPU loaded");
                return "ort_gpu";
            }
            spdlog::warn("[ObjectDetection] ONNX Runtime GPU failed, trying OpenCV DNN CPU");

            // 回退到 OpenCV DNN CPU
            if (m_dnnInference.loadModel(modelPath, configPath)) {
                spdlog::info("[ObjectDetection] OpenCV DNN CPU loaded");
                return "dnn_cpu";
            }

            spdlog::error("[ObjectDetection] all backends failed");
            return "none";
        }
    );

    // 使用 QFutureWatcher 监听完成信号
    QFutureWatcher<QString>* watcher = new QFutureWatcher<QString>(this);
    connect(watcher, &QFutureWatcher<QString>::finished, this, [this, watcher, modelPath]() {
        QString result = watcher->result();
        watcher->deleteLater();
        m_ui->btn_apply->setEnabled(true);

        if (result == "ort_gpu") {
            m_useOrt = true;
            m_currentModelPath = modelPath;
            emit modelLoadFinished(true, "状态：模型加载成功 (ONNX Runtime GPU)");
            qDebug() << "[ObjectDetection] ONNX Runtime GPU loaded successfully";
        } else if (result == "dnn_cpu") {
            m_useOrt = false;
            m_currentModelPath = modelPath;
            emit modelLoadFinished(true, "状态：模型加载成功 (OpenCV DNN CPU)");
            qDebug() << "[ObjectDetection] OpenCV DNN CPU loaded successfully";
        } else {
            emit modelLoadFinished(false, "状态：所有推理后端均失败");
            qDebug() << "[ObjectDetection] all backends failed";
        }
    });
    watcher->setFuture(future);
}

std::vector<DetectionResult> ObjectDetectionTabWidget::runDetection(const cv::Mat& image)
{
    if (image.empty()) {
        return {};
    }

    // 优先使用 ONNX Runtime
    if (m_useOrt && m_ortInference.isLoaded()) {
        return m_ortInference.detect(image,
            getConfidenceThreshold(),
            getNmsThreshold(),
            getInputWidth(),
            getInputHeight());
    }

    // 回退到 OpenCV DNN
    if (m_dnnInference.isLoaded()) {
        return m_dnnInference.detect(image,
            getConfidenceThreshold(),
            getNmsThreshold(),
            getInputWidth(),
            getInputHeight());
    }

    return {};
}

bool ObjectDetectionTabWidget::isModelLoaded() const
{
    return m_ortInference.isLoaded() || m_dnnInference.isLoaded();
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

void ObjectDetectionTabWidget::connectSignals(PipelineManager* pm, RoiManager* rm,
                                              ImageView* view, RoiUiController* roiCtrl,
                                              std::function<void()> requestRefresh,
                                              std::function<void()> processAndDisplay)
{
    Q_UNUSED(pm); Q_UNUSED(rm); Q_UNUSED(view); Q_UNUSED(roiCtrl); Q_UNUSED(processAndDisplay);
    connect(this, &ObjectDetectionTabWidget::detectionConfigChanged,
            this, [requestRefresh]() { requestRefresh(); });
}