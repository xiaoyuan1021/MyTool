#include "widgets/object_detection_tab_widget.h"
#include "ui_object_detection_tab.h"
#include "logger.h"
#include "controllers/roi_ui_controller.h"
#include <QFileDialog>
#include <QMessageBox>
#include <QtConcurrent/QtConcurrent>
#include <QCoreApplication>
#include <QDir>
#include <QTimer>
#include <QFile>

ObjectDetectionTabWidget::ObjectDetectionTabWidget(IPipelineAccess* pipelineAccess, QWidget* parent)
    : QWidget(parent)
    , m_ui(new Ui::ObjectDetectionTabForm)
    , m_pipeline(pipelineAccess)
{
    m_ui->setupUi(this);
    setupConnections();
    autoLoadDefaultModel();
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

    // 模型加载完成信号（仅更新状态，不自动触发检测）
    connect(this, &ObjectDetectionTabWidget::modelLoadFinished, this,
        [this](bool success, const QString& message) {
            m_ui->label_status->setText(message);
            if (success) {
                spdlog::info("[ObjectDetection] model loaded successfully");
            }
        });
}

void ObjectDetectionTabWidget::onBrowseModel()
{
    QString defaultDir = QString(PROJECT_ROOT_DIR) + "/resources/models";
    QString filePath = QFileDialog::getOpenFileName(this,
        "选择模型文件", defaultDir,
        "模型文件 (*.onnx *.prototxt *.pb *.cfg *.weights);;ONNX模型 (*.onnx);;所有文件 (*)");

    if (!filePath.isEmpty()) {
        m_ui->lineEdit_modelPath->setText(filePath);
        loadModelAsync();  // 路径变更后自动加载
    }
}

void ObjectDetectionTabWidget::onBrowseConfig()
{
    QString defaultDir = QString(PROJECT_ROOT_DIR) + "/resources/models";
    QString filePath = QFileDialog::getOpenFileName(this,
        "选择配置文件", defaultDir,
        "配置文件 (*.prototxt *.pbtxt *.cfg);;所有文件 (*)");

    if (!filePath.isEmpty()) {
        m_ui->lineEdit_configPath->setText(filePath);
        loadModelAsync();  // 配置变更后自动加载
    }
}

void ObjectDetectionTabWidget::autoLoadDefaultModel()
{
    // 默认模型路径：项目根目录/resources/models/model_pin/ort/pin.onnx
    QString defaultModelPath = QString(PROJECT_ROOT_DIR) + "/resources/models/model_pin/ort/pin.onnx";

    if (QFile::exists(defaultModelPath)) {
        m_ui->lineEdit_modelPath->setText(defaultModelPath);
        spdlog::debug("[ObjectDetection] auto-loading default model: {}", defaultModelPath.toStdString());

        // 延迟加载模型，等待UI完全初始化
        QTimer::singleShot(100, this, &ObjectDetectionTabWidget::loadModelAsync);
    } else {
        spdlog::debug("[ObjectDetection] default model not found: {}", defaultModelPath.toStdString());
    }
}

void ObjectDetectionTabWidget::loadModelAsync()
{
    QString modelPath = m_ui->lineEdit_modelPath->text().trimmed();
    if (modelPath.isEmpty()) return;

    QString configPath = m_ui->lineEdit_configPath->text().trimmed();

    // 如果模型和配置都已加载且路径相同，无需重新加载
    if (m_dnnInference.isLoaded() && m_currentModelPath == modelPath && m_currentConfigPath == configPath) {
        spdlog::info("[ObjectDetection] model already loaded, skip reloading");
        m_ui->label_status->setText("状态：模型已就绪");
        m_pipeline->updateConfig([this](PipelineConfig& cfg) {
            cfg.objectDetection.expectedCount = m_ui->spinBox_expectedCount->value();
        });
        emit detectionConfigChanged();
        return;
    }

    m_ui->label_status->setText("状态：正在加载模型...");
    m_ui->btn_apply->setEnabled(false);

    QFuture<QString> future = QtConcurrent::run(
        [this, modelPath, configPath]() -> QString {
            // Step 1: 加载 ONNX Runtime GPU（优先，推理更快）
            bool ortOk = m_ortInference.loadModel(modelPath, true);

            // Step 2: 加载 OpenCV DNN（回退，标签页静图检测）
            bool dnnOk = m_dnnInference.loadModel(modelPath, configPath);

            if (ortOk && dnnOk) {
                spdlog::info("[ObjectDetection] ONNX Runtime GPU + OpenCV DNN loaded");
                return "both";
            }
            if (ortOk) {
                spdlog::info("[ObjectDetection] ONNX Runtime GPU loaded, OpenCV DNN unavailable");
                return "ort";
            }
            if (dnnOk) {
                spdlog::warn("[ObjectDetection] OpenCV DNN loaded, ONNX Runtime unavailable");
                return "dnn";
            }
            spdlog::error("[ObjectDetection] all backends failed");
            return "none";
        }
    );

    // 使用 QFutureWatcher 监听完成信号
    QFutureWatcher<QString>* watcher = new QFutureWatcher<QString>(this);
    connect(watcher, &QFutureWatcher<QString>::finished, this, [this, watcher, modelPath, configPath]() {
        QString result = watcher->result();
        watcher->deleteLater();
        m_ui->btn_apply->setEnabled(true);

        if (result == "both" || result == "ort" || result == "dnn") {
            m_currentModelPath = modelPath;
            m_currentConfigPath = configPath;

            // 预热：消除首次推理延迟（优先预热ONNX Runtime GPU）
            cv::Mat warmup(640, 640, CV_8UC3, cv::Scalar(114, 114, 114));
            if (m_ortInference.isLoaded()) {
                m_ortInference.detect(warmup, 0.5f, 0.4f, 640, 640);
                spdlog::info("[ObjectDetection] ONNX Runtime GPU warmup completed");
            } else if (m_dnnInference.isLoaded()) {
                m_dnnInference.detect(warmup, 0.5f, 0.4f, 640, 640);
                spdlog::info("[ObjectDetection] OpenCV DNN warmup completed");
            }

            QString msg;
            if (result == "both") msg = "状态：模型加载成功 (ORT GPU + DNN)";
            else if (result == "ort") msg = "状态：模型加载成功 (ORT GPU)";
            else msg = "状态：模型加载成功 (DNN)";
            
            emit modelLoadFinished(true, msg);
            spdlog::debug("[ObjectDetection] {}", msg.toStdString());
        } else {
            emit modelLoadFinished(false, "状态：所有推理后端均失败");
            spdlog::info("[ObjectDetection] all backends failed");
        }
    });
    watcher->setFuture(future);
}

void ObjectDetectionTabWidget::onApplyClicked()
{
    // 检查模型是否已加载
    if (!isModelLoaded()) {
        m_ui->label_status->setText("状态：请先加载模型");
        spdlog::warn("[ObjectDetection] apply clicked but model not loaded");
        return;
    }

    // 同步配置到pipeline并启用检测
    m_pipeline->updateConfig([this](PipelineConfig& cfg) {
        cfg.enableObjectDetection = true;
        cfg.objectDetection.expectedCount = m_ui->spinBox_expectedCount->value();
    });

    // 触发pipeline执行
    spdlog::info("[ObjectDetection] apply clicked, enabling detection and triggering pipeline");
    emit detectionConfigChanged();
}

std::vector<DetectionResult> ObjectDetectionTabWidget::runDetection(const cv::Mat& image)
{
    if (image.empty()) return {};

    // 优先使用 ONNX Runtime GPU（推理更快）
    if (m_ortInference.isLoaded()) {
        return m_ortInference.detect(image,
            getConfidenceThreshold(),
            getNmsThreshold(),
            640, 640);
    }

    // 回退到 OpenCV DNN
    if (m_dnnInference.isLoaded()) {
        cv::Size inputSize = m_dnnInference.getModelInputSize();
        return m_dnnInference.detect(image,
            getConfidenceThreshold(),
            getNmsThreshold(),
            inputSize.width,
            inputSize.height);
    }

    return {};
}

std::vector<DetectionResult> ObjectDetectionTabWidget::runDetectionOrt(const cv::Mat& image)
{
    if (image.empty()) return {};

    // 优先使用 ONNX Runtime（视频推理加速）
    if (m_ortInference.isLoaded()) {
        return m_ortInference.detect(image,
            getConfidenceThreshold(),
            getNmsThreshold(),
            640, 640);
    }

    // 回退到 OpenCV DNN
    if (m_dnnInference.isLoaded()) {
        cv::Size inputSize = m_dnnInference.getModelInputSize();
        return m_dnnInference.detect(image,
            getConfidenceThreshold(),
            getNmsThreshold(),
            inputSize.width,
            inputSize.height);
    }

    return {};
}

bool ObjectDetectionTabWidget::isModelLoaded() const
{
    return m_dnnInference.isLoaded() || m_ortInference.isLoaded();
}

bool ObjectDetectionTabWidget::isOrtLoaded() const
{
    return m_ortInference.isLoaded();
}

float ObjectDetectionTabWidget::getConfidenceThreshold() const
{
    return m_ui->slider_confidenceThreshold->value() / 100.0f;
}

float ObjectDetectionTabWidget::getNmsThreshold() const
{
    return m_ui->slider_nmsThreshold->value() / 100.0f;
}

ObjectDetectionConfig ObjectDetectionTabWidget::getDetectionConfig() const
{
    ObjectDetectionConfig config;
    config.modelPath = m_ui->lineEdit_modelPath->text();
    config.configPath = m_ui->lineEdit_configPath->text();
    config.confidenceThreshold = getConfidenceThreshold();
    config.nmsThreshold = getNmsThreshold();
    config.expectedCount = m_ui->spinBox_expectedCount->value();
    return config;
}

void ObjectDetectionTabWidget::setDetectionConfig(const ObjectDetectionConfig& config)
{
    QSignalBlocker blocker1(m_ui->lineEdit_modelPath);
    QSignalBlocker blocker2(m_ui->lineEdit_configPath);
    QSignalBlocker blocker3(m_ui->slider_confidenceThreshold);
    QSignalBlocker blocker4(m_ui->slider_nmsThreshold);
    QSignalBlocker blocker5(m_ui->spinBox_expectedCount);

    m_ui->lineEdit_modelPath->setText(config.modelPath);
    m_ui->lineEdit_configPath->setText(config.configPath);
    m_ui->slider_confidenceThreshold->setValue(static_cast<int>(config.confidenceThreshold * 100));
    m_ui->slider_nmsThreshold->setValue(static_cast<int>(config.nmsThreshold * 100));
    m_ui->spinBox_expectedCount->setValue(config.expectedCount);
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

void ObjectDetectionTabWidget::connectSignals(const SignalContext& ctx,
                                               std::function<void()> onExecutePipeline,
                                               std::function<void()> onConfigSaved)
{
    Q_UNUSED(onConfigSaved);
    connect(this, &ObjectDetectionTabWidget::detectionConfigChanged,
            this, [onExecutePipeline]() { onExecutePipeline(); });

    // [NOTE] 所有配置变化时同步到DetectionItem.config
    auto syncToDetectionItem = [this, ctx]() {
        if (ctx.roiCtrl) {
            ctx.roiCtrl->updateObjectDetectionConfig(getDetectionConfig());
        }
    };

    connect(m_ui->lineEdit_modelPath, &QLineEdit::textChanged, this, syncToDetectionItem);
    connect(m_ui->lineEdit_configPath, &QLineEdit::textChanged, this, syncToDetectionItem);
    connect(m_ui->slider_confidenceThreshold, &QSlider::valueChanged, this, [this, syncToDetectionItem](int) { syncToDetectionItem(); });
    connect(m_ui->slider_nmsThreshold, &QSlider::valueChanged, this, [this, syncToDetectionItem](int) { syncToDetectionItem(); });
    connect(m_ui->spinBox_expectedCount, QOverload<int>::of(&QSpinBox::valueChanged), this, [this, syncToDetectionItem](int) { syncToDetectionItem(); });
}