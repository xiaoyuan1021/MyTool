#include "controllers/detection_ui_controller.h"
#include "logger.h"

#include <QMessageBox>
#include <QComboBox>
#include <QVBoxLayout>
#include <QLabel>
#include <QDialog>
#include <QDialogButtonBox>
#include <QApplication>

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

void DetectionUiController::onDeleteDetectionClicked()
{
    // 这个方法需要在主窗口中调用，传入当前选中的检测项
    // 暂时留空，由主窗口直接处理
}

void DetectionUiController::switchToTabConfig(const TabConfig& config)
{
    if (!m_tabWidget) return;
    
    // 隐藏所有Tab
    for (int i = 0; i < m_tabWidget->count(); ++i) {
        m_tabWidget->setTabVisible(i, false);
    }
    
    // 显示配置中指定的Tab（懒加载：如果Tab尚未创建则通知MainWindow创建）
    for (const QString& tabName : config.tabNames) {
        // 请求MainWindow确保此Tab已创建
        emit ensureTabNeeded(tabName);
        
        // 找到并显示对应的Tab
        for (int i = 0; i < m_tabNames.size(); ++i) {
            if (m_tabNames[i] == tabName) {
                m_tabWidget->setTabVisible(i, true);
                break;
            }
        }
    }
    
    // 切换到第一个可见的Tab
    for (int i = 0; i < m_tabWidget->count(); ++i) {
        if (m_tabWidget->isTabVisible(i)) {
            m_tabWidget->setCurrentIndex(i);
            break;
        }
    }
}

