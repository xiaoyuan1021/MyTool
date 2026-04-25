#include "controllers/roi_ui_controller.h"
#include "widgets/roi_list_widget.h"
#include "roi_config.h"
#include "logger.h"
#include "image_utils.h"

#include <QApplication>
#include <QDebug>

RoiUiController::RoiUiController(
    RoiManager& roiManager,
    PipelineManager* pipelineManager,
    ImageView* imageView,
    QStatusBar* statusBar,
    QObject* parent)
    : QObject(parent)
    , m_roiManager(roiManager)
    , m_pipelineManager(pipelineManager)
    , m_view(imageView)
    , m_statusBar(statusBar)
    , m_treeView(nullptr)
    , m_roiListWidget(nullptr)
{
}

RoiUiController::~RoiUiController()
{
}

void RoiUiController::setupTreeView(QTreeWidget* treeView)
{
    m_treeView = treeView;
    
    // 设置树形控件属性
    m_treeView->setHeaderHidden(true);
    m_treeView->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_treeView->setContextMenuPolicy(Qt::CustomContextMenu);
    
    // 连接信号
    connect(m_treeView, &QTreeWidget::itemClicked,
            this, &RoiUiController::onRoiTreeItemClicked);
    connect(m_treeView, &QTreeWidget::itemDoubleClicked,
            this, &RoiUiController::onRoiTreeItemDoubleClicked);
    connect(m_treeView, &QTreeWidget::customContextMenuRequested,
            this, &RoiUiController::onRoiTreeContextMenuRequested);
    
    // 连接绘制ROI完成信号
    connect(m_view, &ImageView::roiSelected, this, [this](const QRectF& rect) {
        // 创建新的ROI配置
        QString roiName = QString("ROI_%1").arg(m_roiManager.getRoiConfigCount() + 1);
        RoiConfig roi(roiName, rect);
        
        // 新ROI继承当前PipelineManager的配置
        if (m_pipelineManager) {
            roi.pipelineConfig = m_pipelineManager->getConfigSnapshot();
        }
        
        m_roiManager.addRoiConfig(roi);
        
        // 更新当前选中的ROI为新创建的ROI
        m_currentSelectedRoiId = roi.roiId;
        
        // 刷新列表
        refreshRoiTreeView();
        
        Logger::instance()->info(QString("已添加ROI: %1").arg(roiName));
    });
    
    // 刷新显示
    refreshRoiTreeView();
}

void RoiUiController::onDrawRoiClicked()
{
    m_roiManager.enableDrawRoiMode(m_view, m_statusBar);
}

void RoiUiController::onResetRoiClicked()
{
    m_roiManager.resetRoiWithUI(m_view, m_statusBar);
    emit roiChanged();
}

void RoiUiController::onAddRoiClicked()
{
    // 检查是否已加载图像
    if (m_roiManager.getFullImage().empty()) {
        QMessageBox::warning(nullptr, "警告", "请先加载一张图像");
        return;
    }
    
    // 提示用户绘制ROI
    Logger::instance()->info("请在图像上绘制新的ROI");
    m_roiManager.enableDrawRoiMode(m_view, m_statusBar);
}

void RoiUiController::onDeleteRoiClicked()
{
    // 检查是否有选中的ROI
    QTreeWidgetItem* currentItem = m_treeView->currentItem();
    if (!currentItem) {
        QMessageBox::warning(nullptr, "警告", "请先选择要删除的ROI");
        return;
    }
    
    // 获取ROI ID
    QString roiId = currentItem->data(0, Qt::UserRole).toString();
    QString itemType = currentItem->data(0, Qt::UserRole + 1).toString();
    
    // 如果选中的是检测项，需要找到其父ROI
    if (itemType == "detection") {
        QTreeWidgetItem* parentItem = currentItem->parent();
        if (parentItem) {
            roiId = parentItem->data(0, Qt::UserRole).toString();
        } else {
            QMessageBox::warning(nullptr, "警告", "请先选择要删除的ROI");
            return;
        }
    }
    
    // 获取ROI配置
    RoiConfig* roi = m_roiManager.getRoiConfig(roiId);
    if (!roi) {
        QMessageBox::warning(nullptr, "警告", "未找到选中的ROI");
        return;
    }
    
    // 确认删除
    QMessageBox::StandardButton reply = QMessageBox::question(
        nullptr, "确认删除",
        QString("确定要删除ROI \"%1\" 及其所有检测项吗？").arg(roi->roiName),
        QMessageBox::Yes | QMessageBox::No
    );
    
    if (reply == QMessageBox::Yes) {
        // 删除ROI
        m_roiManager.removeRoiConfig(roiId);
        
        // 刷新树形视图
        refreshRoiTreeView();
        
        // 重置ROI管理器
        m_roiManager.resetRoiWithUI(m_view, m_statusBar);
        
        // 发出信号通知主窗口更新
        emit roiChanged();
        
        Logger::instance()->info(QString("已删除ROI: %1").arg(roi->roiName));
    }
}

void RoiUiController::onSwitchRoiClicked()
{
    if (m_roiManager.getFullImage().empty()) {
        QMessageBox::warning(nullptr, "警告", "请先加载一张图像");
        return;
    }
    
    if (m_roiManager.isRoiActive()) {
        // 当前是ROI模式，切换到原图模式
        // 先保存当前ROI的PipelineConfig
        saveCurrentRoiPipelineConfig();
        
        m_roiManager.resetRoi();
        m_view->clearRoi();
        m_view->resetZoom();
        m_view->setImage(ImageUtils::matToQImage(m_roiManager.getFullImage()));
        Logger::instance()->info("已切换到原图模式");
    } else {
        // 当前是原图模式，切换到ROI模式
        if (m_currentSelectedRoiId.isEmpty()) {
            QMessageBox::warning(nullptr, "警告", "请先在左侧列表中选择一个ROI");
            return;
        }
        
        RoiConfig* roi = m_roiManager.getRoiConfig(m_currentSelectedRoiId);
        if (!roi) {
            QMessageBox::warning(nullptr, "警告", "未找到选中的ROI");
            return;
        }
        
        // 加载该ROI的PipelineConfig
        loadRoiPipelineConfig(m_currentSelectedRoiId);
        
        // 切换到ROI模式
        m_view->clearRoi();
        m_roiManager.setRoi(roi->roiRect);
        emit roiChanged();
        Logger::instance()->info(QString("已切换到ROI模式: %1").arg(roi->roiName));
    }
}

void RoiUiController::onRoiSelected(const QRectF& roiRect)
{
    if (!m_roiManager.handleRoiSelected(roiRect, m_statusBar)) {
        return;
    }

    // 绘制完ROI后重置图像缩放，恢复到原始比例
    m_view->resetZoom();

    emit roiChanged();
}

void RoiUiController::refreshRoiTreeView()
{
    if (!m_treeView) return;
    
    // 刷新前保存当前ROI的PipelineConfig
    saveCurrentRoiPipelineConfig();
    
    // 保存当前选中的ROI ID，以便刷新后恢复选中状态
    QString previouslySelectedRoiId = m_currentSelectedRoiId;
    
    // 阻塞信号，防止setCurrentItem触发itemClicked递归
    m_treeView->blockSignals(true);
    m_treeView->clear();
    m_treeView->blockSignals(false);
    
    const QList<RoiConfig>& rois = m_roiManager.getRoiConfigs();
    
    QTreeWidgetItem* itemToSelect = nullptr;  // 要选中的项目
    
    for (const auto& roi : rois) {
        // 创建ROI节点
        QTreeWidgetItem* roiItem = new QTreeWidgetItem(m_treeView);
        
        // 显示格式：ROI名称 [检测项类型]
        QString detectionTypes;
        for (int i = 0; i < roi.detectionItems.size(); ++i) {
            if (i > 0) detectionTypes += ", ";
            detectionTypes += detectionTypeToString(roi.detectionItems[i].type);
        }
        
        if (detectionTypes.isEmpty()) {
            roiItem->setText(0, roi.roiName);
        } else {
            roiItem->setText(0, QString("%1 [%2]").arg(roi.roiName).arg(detectionTypes));
        }
        
        roiItem->setData(0, Qt::UserRole, roi.roiId);
        roiItem->setFlags(roiItem->flags() | Qt::ItemIsSelectable | Qt::ItemIsEnabled);
        
        // 设置ROI颜色
        QColor roiColor(roi.color);
        roiItem->setForeground(0, QBrush(roiColor));
        
        // 记录需要选中的项目
        if (roi.roiId == previouslySelectedRoiId) {
            itemToSelect = roiItem;
        }
        
        // 添加检测项子节点（简化显示）
        for (const auto& detection : roi.detectionItems) {
            QTreeWidgetItem* detectionItem = new QTreeWidgetItem(roiItem);
            detectionItem->setText(0, detection.itemName);
            detectionItem->setData(0, Qt::UserRole, detection.itemId);
            detectionItem->setData(0, Qt::UserRole + 1, "detection"); // 标记为检测项
            
            // 根据检测类型设置颜色
            QColor detectionColor;
            switch (detection.type) {
                case DetectionType::Blob:
                    detectionColor = QColor("#4CAF50");
                    break;
                case DetectionType::Line:
                    detectionColor = QColor("#2196F3");
                    break;
                case DetectionType::Barcode:
                    detectionColor = QColor("#FF9800");
                    break;
                default:
                    detectionColor = QColor("#9E9E9E");
                    break;
            }
            detectionItem->setForeground(0, QBrush(detectionColor));
        }
    }
    
    // 展开所有项
    m_treeView->expandAll();
    
    // 恢复选中状态（阻塞信号避免递归触发itemClicked）
    m_treeView->blockSignals(true);
    QString newSelectedId;
    if (itemToSelect) {
        m_treeView->setCurrentItem(itemToSelect);
        newSelectedId = itemToSelect->data(0, Qt::UserRole).toString();
    } else if (m_treeView->topLevelItemCount() > 0) {
        QTreeWidgetItem* lastItem = m_treeView->topLevelItem(
            m_treeView->topLevelItemCount() - 1
        );
        m_treeView->setCurrentItem(lastItem);
        newSelectedId = lastItem->data(0, Qt::UserRole).toString();
    }
    
    // 【Bug3修复】检测选中ID是否发生变化，如果变化了需要加载新ROI的配置
    bool selectionChanged = (newSelectedId != m_currentSelectedRoiId);
    m_currentSelectedRoiId = newSelectedId;
    m_treeView->blockSignals(false);
    
    // 如果选中ID发生了变化（如删除后自动选择了另一个ROI），
    // 需要加载新ROI的PipelineConfig到PipelineManager，确保UI显示正确的配置
    if (selectionChanged && !m_currentSelectedRoiId.isEmpty()) {
        loadRoiPipelineConfig(m_currentSelectedRoiId);
    }
}

void RoiUiController::onRoiTreeItemClicked(QTreeWidgetItem* item, int column)
{
    Q_UNUSED(column);
    
    if (!item) {
        return;
    }
    
    QString itemId = item->data(0, Qt::UserRole).toString();
    QString itemType = item->data(0, Qt::UserRole + 1).toString();
    
    if (itemType == "detection") {
        // 点击的是检测项，找到父ROI
        QTreeWidgetItem* parentItem = item->parent();
        if (parentItem) {
            QString newRoiId = parentItem->data(0, Qt::UserRole).toString();
            
            // 如果切换了ROI，先保存旧ROI的配置，再加载新ROI的配置
            if (newRoiId != m_currentSelectedRoiId) {
                saveCurrentRoiPipelineConfig();
                m_currentSelectedRoiId = newRoiId;
                loadRoiPipelineConfig(m_currentSelectedRoiId);
            }
            
            // 发出检测项选中信号，用于触发Tab切换
            emit detectionItemSelected(m_currentSelectedRoiId, itemId);
        }
        
        Logger::instance()->info(QString("选中检测项: %1").arg(item->text(0)));
    } else {
        // 点击的是ROI
        QString newRoiId = itemId;
        
        // 如果切换了ROI，先保存旧ROI的配置，再加载新ROI的配置
        if (newRoiId != m_currentSelectedRoiId) {
            saveCurrentRoiPipelineConfig();
            m_currentSelectedRoiId = newRoiId;
            loadRoiPipelineConfig(m_currentSelectedRoiId);
        } else {
            m_currentSelectedRoiId = newRoiId;
        }
        
        // 在图像视图中显示对应的ROI
        RoiConfig* roi = m_roiManager.getRoiConfig(m_currentSelectedRoiId);
        if (roi) {
            m_view->clearRoi();
            m_roiManager.setRoi(roi->roiRect);
            
            emit roiChanged();
            Logger::instance()->info(QString("已选中ROI: %1").arg(roi->roiName));
        } else {
            Logger::instance()->info(QString("选中ROI: %1").arg(itemId));
        }
    }
}

void RoiUiController::onRoiTreeItemDoubleClicked(QTreeWidgetItem* item, int column)
{
    Q_UNUSED(column);
    Q_UNUSED(item);
}

void RoiUiController::onRoiTreeContextMenuRequested(const QPoint& pos)
{
    Q_UNUSED(pos);
}

// ========== ROI操作方法（供MainWindow委托调用）==========

void RoiUiController::handleRoiSelectedComplete(const QRectF& roiImgRectF)
{
    if (!m_roiManager.handleRoiSelected(roiImgRectF, m_statusBar)) {
        return;
    }
    
    // 绘制完ROI后重置图像缩放，恢复到原始比例
    m_view->resetZoom();
    
    emit roiChanged();
}

QString RoiUiController::addRoiWithName(const QString& roiName)
{
    if (m_roiManager.getFullImage().empty()) {
        QMessageBox::warning(nullptr, "警告", "请先加载一张图像");
        return QString();
    }
    
    // 进入绘制模式
    Logger::instance()->info(QString("请在图像上绘制新的ROI: %1").arg(roiName));
    m_roiManager.enableDrawRoiMode(m_view, m_statusBar);
    
    return roiName;
}

bool RoiUiController::renameRoi(const QString& roiId, const QString& newName)
{
    RoiConfig* roi = m_roiManager.getRoiConfig(roiId);
    if (!roi) {
        return false;
    }
    
    roi->roiName = newName;
    refreshRoiTreeView();
    emit roiChanged();
    emit roiRenamed(roiId, newName);
    
    Logger::instance()->info(QString("已重命名ROI: %1 -> %2").arg(roi->roiName).arg(newName));
    return true;
}

bool RoiUiController::toggleRoiActive(const QString& roiId)
{
    RoiConfig* roi = m_roiManager.getRoiConfig(roiId);
    if (!roi) {
        return false;
    }
    
    roi->isActive = !roi->isActive;
    refreshRoiTreeView();
    emit roiChanged();
    emit roiActiveToggled(roiId, roi->isActive);
    
    Logger::instance()->info(QString("ROI %1 已%2").arg(roi->roiName).arg(roi->isActive ? "激活" : "停用"));
    return true;
}

void RoiUiController::deleteDetectionItem(const QString& roiId, const QString& detectionId)
{
    RoiConfig* roi = m_roiManager.getRoiConfig(roiId);
    if (!roi) {
        QMessageBox::warning(nullptr, "警告", "未找到选中的ROI");
        return;
    }
    
    // 查找检测项名称用于确认提示
    QString detectionName;
    for (const auto& detection : roi->detectionItems) {
        if (detection.itemId == detectionId) {
            detectionName = detection.itemName;
            break;
        }
    }
    
    // 确认删除
    QMessageBox::StandardButton reply = QMessageBox::question(
        nullptr, "确认删除",
        QString("确定要删除检测项 \"%1\" 吗？").arg(detectionName.isEmpty() ? detectionId : detectionName),
        QMessageBox::Yes | QMessageBox::No
    );
    
    if (reply == QMessageBox::Yes) {
        roi->removeDetectionItem(detectionId);
        refreshRoiTreeView();
        emit detectionItemDeleted(roiId, detectionId);
        Logger::instance()->info(QString("已删除检测项: %1").arg(detectionName.isEmpty() ? detectionId : detectionName));
    }
}

// ========== RoiListWidget设置和信号连接 ==========

void RoiUiController::setupRoiListWidget(RoiListWidget* roiListWidget)
{
    m_roiListWidget = roiListWidget;
    
    if (!m_roiListWidget) {
        return;
    }
    
    // 连接RoiListWidget信号到Controller处理
    connect(m_roiListWidget, &RoiListWidget::roiAddRequested,
            this, &RoiUiController::handleRoiAddRequested);
    connect(m_roiListWidget, &RoiListWidget::roiDeleteRequested,
            this, &RoiUiController::handleRoiDeleteRequested);
    connect(m_roiListWidget, &RoiListWidget::roiRenameRequested,
            this, &RoiUiController::handleRoiRenameRequested);
    connect(m_roiListWidget, &RoiListWidget::roiActiveChanged,
            this, &RoiUiController::handleRoiActiveChanged);
    connect(m_roiListWidget, &RoiListWidget::roiSelectionChanged,
            this, &RoiUiController::handleRoiSelectionChanged);
    connect(m_roiListWidget, &RoiListWidget::drawRoiRequested,
            this, &RoiUiController::onDrawRoiClicked);
    
    // 连接RoiManager信号到RoiListWidget数据同步
    connect(&m_roiManager, &RoiManager::roiConfigChanged,
            this, [this]() {
                if (m_roiListWidget) {
                    m_roiListWidget->setRoiData(m_roiManager.getRoiConfigs());
                }
            });
    
    // 初始同步数据
    m_roiListWidget->setRoiData(m_roiManager.getRoiConfigs());
}

// ========== RoiListWidget信号处理槽函数 ==========

void RoiUiController::handleRoiAddRequested(const QString& roiName)
{
    // 检查图像是否已加载
    if (m_roiManager.getFullImage().empty()) {
        QMessageBox::warning(nullptr, "警告", "请先加载一张图像");
        return;
    }
    
    // 保存当前ROI的配置
    saveCurrentRoiPipelineConfig();
    
    // 进入绘制模式
    Logger::instance()->info("请在图像上绘制新的ROI");
    m_roiManager.enableDrawRoiMode(m_view, m_statusBar);
}

void RoiUiController::handleRoiDeleteRequested(const QString& roiId)
{
    Logger::instance()->info(QString("[RoiUI] handleRoiDeleteRequested: roiId=%1, 当前选中=%2").arg(roiId).arg(m_currentSelectedRoiId));
    
    // 获取ROI名称用于日志
    RoiConfig* roi = m_roiManager.getRoiConfig(roiId);
    QString roiName = roi ? roi->roiName : roiId;
    
    // 记录删除前是否是当前选中的ROI
    bool wasSelected = (m_currentSelectedRoiId == roiId);
    Logger::instance()->info(QString("[RoiUI] 删除前: roiName=%1, wasSelected=%2, ROI配置数=%3")
        .arg(roiName).arg(wasSelected ? "true" : "false").arg(m_roiManager.getRoiConfigCount()));
    
    if (m_roiManager.removeRoiConfig(roiId)) {
        Logger::instance()->info(QString("[RoiUI] 删除成功, 剩余ROI配置数=%1").arg(m_roiManager.getRoiConfigCount()));
        
        // 如果删除的是当前选中的ROI，重置ROI状态
        if (wasSelected) {
            Logger::instance()->info("[RoiUI] 删除的是当前选中ROI，重置ROI状态");
            m_roiManager.resetRoiWithUI(m_view, m_statusBar);
            m_currentSelectedRoiId.clear();
        }
        
        // 【Bug3修复】刷新后，如果自动选择了新的ROI，需要加载其PipelineConfig
        QString previousSelectedId = m_currentSelectedRoiId;
        Logger::instance()->info(QString("[RoiUI] refreshRoiTreeView前: selectedId=%1").arg(m_currentSelectedRoiId));
        refreshRoiTreeView();
        Logger::instance()->info(QString("[RoiUI] refreshRoiTreeView后: selectedId=%1").arg(m_currentSelectedRoiId));
        
        // 如果auto-selection改变了选中的ROI，加载新ROI的配置
        if (m_currentSelectedRoiId != previousSelectedId && !m_currentSelectedRoiId.isEmpty()) {
            Logger::instance()->info(QString("[RoiUI] 选中ID变化: %1 -> %2, 加载新ROI的PipelineConfig")
                .arg(previousSelectedId).arg(m_currentSelectedRoiId));
            loadRoiPipelineConfig(m_currentSelectedRoiId);
        }
        
        emit roiChanged();
        Logger::instance()->info(QString("已删除ROI: %1").arg(roiName));
    } else {
        Logger::instance()->warning(QString("[RoiUI] 删除失败: roiId=%1").arg(roiId));
    }
}

void RoiUiController::handleRoiRenameRequested(const QString& roiId, const QString& newName)
{
    renameRoi(roiId, newName);
}

void RoiUiController::handleRoiActiveChanged(const QString& roiId, bool active)
{
    RoiConfig* roi = m_roiManager.getRoiConfig(roiId);
    if (roi) {
        roi->isActive = active;
        refreshRoiTreeView();
        emit roiChanged();
        emit roiActiveToggled(roiId, active);
        
        Logger::instance()->info(QString("ROI %1 已%2").arg(roi->roiName).arg(active ? "激活" : "停用"));
    }
}

void RoiUiController::handleRoiSelectionChanged(const QString& roiId)
{
    if (roiId == m_currentSelectedRoiId) {
        return;
    }
    
    // 保存旧ROI的配置
    saveCurrentRoiPipelineConfig();
    
    m_currentSelectedRoiId = roiId;
    
    // 加载新ROI的配置
    loadRoiPipelineConfig(roiId);
    
    // 在图像视图中显示对应的ROI
    RoiConfig* roi = m_roiManager.getRoiConfig(roiId);
    if (roi) {
        m_view->clearRoi();
        m_roiManager.setRoi(roi->roiRect);
        
        emit roiChanged();
        Logger::instance()->info(QString("已选中ROI: %1").arg(roi->roiName));
    }
}

void RoiUiController::saveCurrentRoiPipelineConfig()
{
    if (m_currentSelectedRoiId.isEmpty() || !m_pipelineManager) {
        Logger::instance()->info(QString("[RoiUI] saveCurrentRoiPipelineConfig: 跳过 (selectedId=%1, pipeline=%2)")
            .arg(m_currentSelectedRoiId).arg(m_pipelineManager ? "有效" : "nullptr"));
        return;
    }
    
    RoiConfig* roi = m_roiManager.getRoiConfig(m_currentSelectedRoiId);
    if (roi) {
        roi->pipelineConfig = m_pipelineManager->getConfigSnapshot();
        Logger::instance()->info(QString("[RoiUI] 已保存ROI [%1] 的Pipeline配置: brightness=%2, contrast=%3, gamma=%4")
            .arg(roi->roiName)
            .arg(roi->pipelineConfig.brightness)
            .arg(roi->pipelineConfig.contrast)
            .arg(roi->pipelineConfig.gamma));
    } else {
        Logger::instance()->warning(QString("[RoiUI] saveCurrentRoiPipelineConfig: ROI不存在, id=%1").arg(m_currentSelectedRoiId));
    }
}

void RoiUiController::loadRoiPipelineConfig(const QString& roiId)
{
    if (roiId.isEmpty() || !m_pipelineManager) {
        Logger::instance()->info(QString("[RoiUI] loadRoiPipelineConfig: 跳过 (roiId=%1, pipeline=%2)")
            .arg(roiId).arg(m_pipelineManager ? "有效" : "nullptr"));
        return;
    }
    
    RoiConfig* roi = m_roiManager.getRoiConfig(roiId);
    if (roi) {
        m_pipelineManager->setConfig(roi->pipelineConfig);
        
        emit roiPipelineConfigChanged(roi->pipelineConfig);
        
        Logger::instance()->info(QString("[RoiUI] 已加载ROI [%1] 的Pipeline配置: brightness=%2, contrast=%3, gamma=%4, sharpen=%5, channel=%6")
            .arg(roi->roiName)
            .arg(roi->pipelineConfig.brightness)
            .arg(roi->pipelineConfig.contrast)
            .arg(roi->pipelineConfig.gamma)
            .arg(roi->pipelineConfig.sharpen)
            .arg(static_cast<int>(roi->pipelineConfig.channel)));
    } else {
        Logger::instance()->warning(QString("[RoiUI] loadRoiPipelineConfig: ROI不存在, id=%1").arg(roiId));
    }
}

// ========== 图片切换时的ROI配置同步 ==========

void RoiUiController::syncRoiConfigsToWidget()
{
    if (m_roiListWidget) {
        m_roiListWidget->setRoiData(m_roiManager.getRoiConfigs());
    }
    refreshRoiTreeView();
    Logger::instance()->info("[RoiUiController] 已同步ROI配置到Widget");
}
