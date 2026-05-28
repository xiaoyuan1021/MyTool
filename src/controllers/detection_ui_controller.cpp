#include "controllers/detection_ui_controller.h"
#include "controllers/roi_ui_controller.h"
#include "config/detection_config_types.h"
#include "logger.h"
#include "widgets/tab_manager.h"
#include "widgets/judge_tab_widget.h"

#include <QMessageBox>
#include <QComboBox>
#include <QVBoxLayout>
#include <QLabel>
#include <QDialog>
#include <QDialogButtonBox>
#include <QApplication>
#include <QSignalBlocker>

DetectionUiController::DetectionUiController(
    RoiManager& roiManager,
    QTabWidget* tabWidget,
    QObject* parent)
    : QObject(parent)
    , m_roiManager(roiManager)
    , m_tabWidget(tabWidget)
{
}

DetectionUiController::~DetectionUiController()
{
}

void DetectionUiController::onAddDetectionClicked(const QString& roiId)
{
    qDebug() << "[DEBUG] onAddDetectionClicked: roiId =" << roiId;
    
    // 检查ROI ID是否有效
    if (roiId.isEmpty()) {
        QMessageBox::warning(nullptr, "警告", "请先在左侧列表中选择一个ROI");
        return;
    }
    
    // 获取ROI配置
    RoiConfig* roi = m_roiManager.getRoiConfig(roiId);
    if (!roi) {
        qDebug() << "[DEBUG] onAddDetectionClicked: ROI not found for ID =" << roiId;
        QMessageBox::warning(nullptr, "警告", "未找到选中的ROI");
        return;
    }
    
    qDebug() << "[DEBUG] onAddDetectionClicked: Found ROI =" << roi->roiName << ", ID =" << roi->roiId;
    
    // 创建对话框
    QDialog dialog;
    dialog.setWindowTitle("添加检测项");
    dialog.setMinimumWidth(250);
    dialog.setWindowFlags(dialog.windowFlags() & ~Qt::WindowContextHelpButtonHint);
    
    // 创建布局
    QVBoxLayout* layout = new QVBoxLayout(&dialog);
    layout->setContentsMargins(20, 20, 20, 20);
    layout->setSpacing(15);
    
    // 添加标签
    QLabel* label = new QLabel("请选择检测项类型:", &dialog);
    layout->addWidget(label);
    
    // 创建ComboBox
    QComboBox* comboBox = new QComboBox(&dialog);
    comboBox->addItem("Blob分析", static_cast<int>(DetectionType::Blob));
    comboBox->addItem("直线检测", static_cast<int>(DetectionType::Line));
    comboBox->addItem("条码识别", static_cast<int>(DetectionType::Barcode));
    comboBox->addItem("目标检测", static_cast<int>(DetectionType::ObjectDetection));
    comboBox->addItem("视频检测", static_cast<int>(DetectionType::VideoDetection));
    comboBox->addItem("自定义Pipeline", static_cast<int>(DetectionType::Custom));
    comboBox->addItem("OCR识别", static_cast<int>(DetectionType::Ocr));
    comboBox->setMinimumWidth(150);
    layout->addWidget(comboBox);
    
    // 添加按钮
    QDialogButtonBox* buttonBox = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    layout->addWidget(buttonBox);
    
    // 连接按钮信号
    connect(buttonBox, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    
    // 显示对话框（自动居中）
    if (dialog.exec() == QDialog::Accepted) {
        int index = comboBox->currentIndex();
        if (index >= 0) {
            DetectionType type = static_cast<DetectionType>(comboBox->currentData().toInt());
            
            // 创建检测项
            QString typeName = detectionTypeToString(type);
            DetectionItem item(typeName, type);
            
            qDebug() << "[DEBUG] onAddDetectionClicked: Adding detection item to ROI =" << roi->roiName;
            
            // 添加到ROI
            roi->addDetectionItem(item);
            
            emit detectionChanged();
            
            Logger::instance()->info(QString("已为ROI %1 添加检测项: %2").arg(roi->roiName).arg(typeName));
        }
    }
}

void DetectionUiController::onDeleteDetectionClicked(const QString& roiId, const QString& detectionId)
{
    if (roiId.isEmpty() || detectionId.isEmpty()) {
        QMessageBox::warning(nullptr, "警告", "请先选择要删除的检测项");
        return;
    }

    RoiConfig* roi = m_roiManager.getRoiConfig(roiId);
    if (!roi) return;

    roi->removeDetectionItem(detectionId);
    emit detectionChanged();

    // 触发Pipeline重新执行，刷新画布显示
    if (m_triggerPipeline) {
        m_triggerPipeline();
    }

    Logger::instance()->info(QString("已删除检测项: %1").arg(detectionId));
}

void DetectionUiController::onDetectionItemSelected(const QString& roiId, const QString& detectionId,
                                                    TabManager* tabManager, PipelineManager* pipelineManager)
{
    RoiConfig* roi = m_roiManager.getRoiConfig(roiId);
    if (!roi) return;

    for (const auto& detection : roi->detectionItems) {
        if (detection.itemId != detectionId) continue;

        TabConfig config = TabConfig::getConfigForType(detection.type);
        switchToTabConfig(config);

        // 根据检测类型启用对应的Pipeline步骤
        pipelineManager->updateConfig([&](PipelineConfig& pipelineConfig) {
            // 先禁用所有步骤
            for (int i = 0; i < PipelineConfig::STEP_COUNT; ++i) {
                pipelineConfig.stepEnabled[i] = false;
            }

            // 根据检测类型启用必要步骤
            switch (detection.type) {
                case DetectionType::Blob:
                    pipelineConfig.stepEnabled[static_cast<int>(StepType::ColorChannel)] = true;
                    pipelineConfig.stepEnabled[static_cast<int>(StepType::Enhance)] = true;
                    pipelineConfig.stepEnabled[static_cast<int>(StepType::Filter)] = true;
                    pipelineConfig.stepEnabled[static_cast<int>(StepType::AlgorithmQueue)] = true;
                    pipelineConfig.stepEnabled[static_cast<int>(StepType::ShapeFilter)] = true;
                    break;
                case DetectionType::Line:
                    pipelineConfig.stepEnabled[static_cast<int>(StepType::ColorChannel)] = true;
                    pipelineConfig.stepEnabled[static_cast<int>(StepType::Enhance)] = true;
                    pipelineConfig.stepEnabled[static_cast<int>(StepType::LineDetector)] = true;
                    break;
                case DetectionType::Barcode:
                    pipelineConfig.stepEnabled[static_cast<int>(StepType::ColorChannel)] = true;
                    pipelineConfig.stepEnabled[static_cast<int>(StepType::Enhance)] = true;
                    pipelineConfig.stepEnabled[static_cast<int>(StepType::BarcodeRecognition)] = true;
                    break;
                case DetectionType::Ocr:
                    pipelineConfig.stepEnabled[static_cast<int>(StepType::ColorChannel)] = true;
                    pipelineConfig.stepEnabled[static_cast<int>(StepType::Enhance)] = true;
                    pipelineConfig.stepEnabled[static_cast<int>(StepType::OcrRecognition)] = true;
                    break;
                default:
                    break;
            }
        });

        // 重建Pipeline
        pipelineManager->rebuildPipeline();

        if (detection.type == DetectionType::Blob) {
            BlobAnalysisConfig blobConfig;
            blobConfig.fromJson(detection.config);
            if (auto* judgeTab = tabManager->getTabAs<JudgeTabWidget>("判定")) {
                judgeTab->setJudgeConfig(
                    blobConfig.minBlobCount,
                    blobConfig.maxBlobCount,
                    pipelineManager->getLastContext().regionCount
                );
            }
        }

        // 将检测项的独立配置同步到Pipeline全局配置
        if (detection.type == DetectionType::Ocr) {
            OcrDetectionConfig ocrCfg;
            ocrCfg.fromJson(detection.config);
            pipelineManager->updateConfig([&](PipelineConfig& pipelineConfig) {
                pipelineConfig.ocr.language = ocrCfg.language;
                pipelineConfig.ocr.pageMode = ocrCfg.pageMode;
                pipelineConfig.ocr.dpi = ocrCfg.dpi;
                pipelineConfig.ocr.confidenceThreshold = ocrCfg.confidenceThreshold;
                pipelineConfig.ocr.whitelist = ocrCfg.whitelist;
                pipelineConfig.enhance = ocrCfg.enhance;
            });
        }

        break;
    }
}

void DetectionUiController::switchToTabConfig(const TabConfig& config)
{
    if (!m_tabWidget) return;

    // 先确保所有需要的Tab都已创建（懒加载）
    for (const QString& tabName : config.tabNames) {
        emit ensureTabNeeded(tabName);
    }

    // 阻塞信号，避免多次 setTabVisible / setCurrentIndex 触发中间状态的 onTabChanged
    const QSignalBlocker blocker(m_tabWidget);

    // 用 tabText 直接比较，不依赖 m_tabNames 索引（避免索引不同步问题）
    for (int i = 0; i < m_tabWidget->count(); ++i) {
        bool shouldShow = config.tabNames.contains(m_tabWidget->tabText(i));
        m_tabWidget->setTabVisible(i, shouldShow);
    }

    // 切换到第一个可见的Tab
    for (int i = 0; i < m_tabWidget->count(); ++i) {
        if (m_tabWidget->isTabVisible(i)) {
            m_tabWidget->setCurrentIndex(i);
            break;
        }
    }

    // blocker 析构后信号恢复，此时 QTabWidget 已处于正确状态
    // 手动触发一次显示模式更新（因为 blockSignals 期间 onTabChanged 没有执行）
    // 注意：这会在 DisplayModeManager 中根据当前 Tab 设置正确的显示模式
    int currentIdx = m_tabWidget->currentIndex();
    if (currentIdx >= 0) {
        // 通过直接调用处理显示模式更新
        QString tabName = m_tabWidget->tabText(currentIdx);
        Logger::instance()->info(QString("[DetectionUi] Tab切换完成: %1").arg(tabName));
    }
}

// ========== UI信号连接（从MainWindow迁移） ==========

bool DetectionUiController::handleDeleteFromTree(QTreeWidget* treeWidget)
{
    if (!treeWidget) return false;

    QTreeWidgetItem* currentItem = treeWidget->currentItem();
    if (!currentItem) {
        QMessageBox::warning(nullptr, "警告", "请先选择要删除的检测项");
        return false;
    }

    if (currentItem->data(0, Qt::UserRole + 1).toString() == "detection") {
        QTreeWidgetItem* parentItem = currentItem->parent();
        if (parentItem) {
            onDeleteDetectionClicked(
                parentItem->data(0, Qt::UserRole).toString(),
                currentItem->data(0, Qt::UserRole).toString());
            return true;
        }
    } else {
        QMessageBox::warning(nullptr, "警告", "请先选择要删除的检测项");
    }
    return false;
}

void DetectionUiController::setupConnections(
    RoiUiController* roiController, std::function<void(const QString&)> ensureTabFunc,
    std::function<void()> triggerPipeline)
{
    m_triggerPipeline = triggerPipeline;

    // 检测项变更时刷新ROI树形视图
    connect(this, &DetectionUiController::detectionChanged,
            this, [roiController]() {
        if (roiController) {
            roiController->refreshRoiTreeView();
        }
    });

    // Tab懒加载请求
    connect(this, &DetectionUiController::ensureTabNeeded,
            this, [ensureTabFunc](const QString& tabName) {
        if (ensureTabFunc) {
            ensureTabFunc(tabName);
        }
    });
}

