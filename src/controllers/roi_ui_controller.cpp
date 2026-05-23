#include "controllers/roi_ui_controller.h"
#include "roi_config.h"
#include "logger.h"
#include "image_utils.h"
#include "widgets/enhance_tab_widget.h"
#include "config/detection_config_types.h"

#include "widgets/tab_manager.h"

#include <QApplication>
#include <QDebug>
#include <QMenu>
#include <QInputDialog>
#include <QMessageBox>

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
    connect(m_treeView, &QTreeWidget::customContextMenuRequested,
            this, &RoiUiController::onRoiTreeContextMenu);
    // 连接绘制ROI完成信号
    connect(m_view, &ImageView::roiSelected, this, [this](const QRectF& rect) {
        // 创建新的ROI配置（名称由RoiManager统一生成，基于现有ROI最大编号）
        QString roiName = m_roiManager.generateNextRoiName();
        RoiConfig roi(roiName, rect);
        
        // 新ROI继承当前PipelineManager的配置
        if (m_pipelineManager) {
            roi.pipelineConfig = m_pipelineManager->getConfigSnapshot();
        }
        
        m_roiManager.addRoiConfig(roi);
        
        // 更新当前选中的ROI为新创建的ROI
        m_currentSelectedRoiId = roi.roiId;
        
        // 【Bug修复】通过ID直接激活，避免setRoi的矩形匹配导致重复创建
        m_view->clearRoi();
        m_roiManager.setActiveRoi(roi.roiId);
        
        // 【P1修复】退出ROI绘制模式
        m_view->finishRoiMode();
        
        // 【P1修复】更新状态栏，清除"请按下左键绘制ROI"的提示
        if (m_statusBar) {
            m_statusBar->showMessage(
                QString("ROI已添加: %1 (x=%2 y=%3 w=%4 h=%5)")
                    .arg(roi.roiName)
                    .arg((int)rect.x()).arg((int)rect.y())
                    .arg((int)rect.width()).arg((int)rect.height()),
                3000
            );
        }
        
        // 刷新列表
        refreshRoiTreeView();

        // 显示ROI裁剪区域并铺满画布（不触发Pipeline检测）
        emit roiDisplayChanged(m_currentSelectedRoiId);
        m_view->resetZoom();

        Logger::instance()->info(QString("已添加ROI: %1").arg(roiName));
    });
    
    // 刷新显示
    refreshRoiTreeView();
}

void RoiUiController::enterRoiDrawMode()
{
    m_view->setRoiMode(true);
    m_view->setDragMode(QGraphicsView::NoDrag);
    if (m_statusBar) m_statusBar->showMessage("请按下左键绘制ROI");
}

void RoiUiController::removeRoiAndRefresh(const QString& roiId)
{
    RoiConfig* roi = m_roiManager.getRoiConfig(roiId);
    QString roiName = roi ? roi->roiName : roiId;
    bool wasSelected = (m_currentSelectedRoiId == roiId);

    if (!m_roiManager.removeRoiConfig(roiId)) {
        Logger::instance()->warning(QString("[RoiUI] 删除失败: roiId=%1").arg(roiId));
        return;
    }

    if (wasSelected) {
        m_roiManager.resetRoi();
        m_view->clearRoi();
        if (m_statusBar) m_statusBar->showMessage("ROI已重置，处理使用完整图像", 2000);
        m_currentSelectedRoiId.clear();
    }

    QString previousSelectedId = m_currentSelectedRoiId;
    refreshRoiTreeView();

    // 如果自动选择改变了选中的ROI，加载新ROI的配置
    if (m_currentSelectedRoiId != previousSelectedId && !m_currentSelectedRoiId.isEmpty()) {
        loadRoiPipelineConfig(m_currentSelectedRoiId);
    }

    emit roiChanged();
    Logger::instance()->info(QString("已删除ROI: %1").arg(roiName));
}

void RoiUiController::onDrawRoiClicked()
{
    if (m_roiManager.getFullImage().empty()) return;
    enterRoiDrawMode();
}

void RoiUiController::onAddRoiClicked()
{
    if (m_roiManager.getFullImage().empty()) {
        QMessageBox::warning(nullptr, "警告", "请先加载一张图像");
        return;
    }
    Logger::instance()->info("请在图像上绘制新的ROI");
    enterRoiDrawMode();
}

QString RoiUiController::addFullImageRoi()
{
    cv::Mat fullImage = m_roiManager.getFullImage();
    if (fullImage.empty()) {
        QMessageBox::warning(nullptr, "警告", "请先加载一张图像");
        return QString();
    }

    // 创建覆盖全图的ROI
    QRectF fullRect(0, 0, fullImage.cols, fullImage.rows);
    RoiConfig roi("整图", fullRect);
    roi.color = "#9B59B6";

    if (m_pipelineManager) {
        roi.pipelineConfig = m_pipelineManager->getConfigSnapshot();
    }

    m_roiManager.addRoiConfig(roi);
    m_currentSelectedRoiId = roi.roiId;
    m_roiManager.setActiveRoi(roi.roiId);
    m_view->clearRoi();
    m_view->finishRoiMode();
    refreshRoiTreeView();

    emit roiDisplayChanged(m_currentSelectedRoiId);
    m_view->resetZoom();

    if (m_statusBar) {
        m_statusBar->showMessage(QString("已创建整图ROI: %1 (w=%2 h=%3)")
            .arg(roi.roiName).arg(fullImage.cols).arg(fullImage.rows), 3000);
    }

    Logger::instance()->info(QString("已创建整图ROI: %1").arg(roi.roiName));
    return roi.roiId;
}

void RoiUiController::onDeleteRoiClicked()
{
    QTreeWidgetItem* currentItem = m_treeView->currentItem();
    if (!currentItem) {
        QMessageBox::warning(nullptr, "警告", "请先选择要删除的ROI");
        return;
    }

    // 获取ROI ID（如果选中的是检测项，找到父ROI）
    QString roiId = currentItem->data(0, Qt::UserRole).toString();
    QString itemType = currentItem->data(0, Qt::UserRole + 1).toString();
    if (itemType == "detection") {
        QTreeWidgetItem* parentItem = currentItem->parent();
        if (parentItem) {
            roiId = parentItem->data(0, Qt::UserRole).toString();
        } else {
            QMessageBox::warning(nullptr, "警告", "请先选择要删除的ROI");
            return;
        }
    }

    // 确认删除
    RoiConfig* roi = m_roiManager.getRoiConfig(roiId);
    if (!roi) {
        QMessageBox::warning(nullptr, "警告", "未找到选中的ROI");
        return;
    }
    QMessageBox::StandardButton reply = QMessageBox::question(
        nullptr, "确认删除",
        QString("确定要删除ROI \"%1\" 及其所有检测项吗？").arg(roi->roiName),
        QMessageBox::Yes | QMessageBox::No);

    if (reply == QMessageBox::Yes) {
        removeRoiAndRefresh(roiId);
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
        
        // 【P1修复】纯显示切换，不触发Pipeline处理
        emit fullImageDisplayRequested();
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
        // 已知ROI ID时直接使用setActiveRoi，避免setRoi的矩形匹配问题
        m_roiManager.setActiveRoi(m_currentSelectedRoiId);
        
        // 【P1修复】纯显示切换，不触发Pipeline处理
        emit roiDisplayChanged(m_currentSelectedRoiId);
        Logger::instance()->info(QString("已切换到ROI模式: %1 (通过ID激活)").arg(roi->roiName));
    }
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
    
    const QList<RoiConfig> rois = m_roiManager.getRoiConfigs();
    
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
                case DetectionType::Ocr:
                    detectionColor = QColor("#9C27B0");
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
        
        // 【P1修复】同步激活ROI到图像视图，保持树形视图与图像显示一致
        RoiConfig* roi = m_roiManager.getRoiConfig(m_currentSelectedRoiId);
        if (roi) {
            m_view->clearRoi();
            // 已知ROI ID时直接使用setActiveRoi，避免setRoi的矩形匹配问题
            m_roiManager.setActiveRoi(roi->roiId);
            emit roiDisplayChanged(m_currentSelectedRoiId);
        }
    } else if (selectionChanged && m_currentSelectedRoiId.isEmpty()) {
        // 【Bug修复】新图片没有ROI时，复位Pipeline配置为默认值，
        // 防止旧图片的增强参数（如brightness=-28）残留在PipelineManager中，
        // 导致新图片运行Pipeline时发黑或显示异常
        if (m_pipelineManager) {
            m_pipelineManager->resetConfigToDefaults();
            emit roiPipelineConfigChanged(m_pipelineManager->getConfigSnapshot());
            Logger::instance()->info("[RoiUI] 无ROI，已复位配置为默认值");
        }
    }
}

void RoiUiController::handleRoiSelection(const QString& roiId)
{
    if (roiId.isEmpty()) return;

    // 如果切换了ROI，先保存旧ROI的配置，再加载新ROI的配置
    if (roiId != m_currentSelectedRoiId) {
        saveCurrentRoiPipelineConfig();
        m_currentSelectedRoiId = roiId;
        loadRoiPipelineConfig(m_currentSelectedRoiId);
    } else {
        m_currentSelectedRoiId = roiId;
    }

    // 激活ROI在图像视图中（纯显示切换，不触发Pipeline处理）
    RoiConfig* roi = m_roiManager.getRoiConfig(m_currentSelectedRoiId);
    if (roi) {
        m_view->clearRoi();
        m_roiManager.setActiveRoi(roi->roiId);
        emit roiDisplayChanged(m_currentSelectedRoiId);
        Logger::instance()->info(QString("已选中ROI: %1 (通过ID激活)").arg(roi->roiName));
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
        // 点击的是检测项，找到父ROI并统一处理选择逻辑
        QTreeWidgetItem* parentItem = item->parent();
        if (parentItem) {
            QString parentRoiId = parentItem->data(0, Qt::UserRole).toString();
            handleRoiSelection(parentRoiId);

            // 发出检测项选中信号，用于触发Tab切换
            emit detectionItemSelected(m_currentSelectedRoiId, itemId);
        }
        Logger::instance()->info(QString("选中检测项: %1").arg(item->text(0)));
    } else {
        // 点击的是ROI，统一处理选择逻辑
        handleRoiSelection(itemId);
    }
}

void RoiUiController::onRoiTreeContextMenu(const QPoint& pos)
{
    QTreeWidgetItem* item = m_treeView->itemAt(pos);
    if (!item) return;

    // 只处理 ROI 层（非检测项子节点）
    QString itemType = item->data(0, Qt::UserRole + 1).toString();
    if (itemType == "detection") return;

    QString roiId = item->data(0, Qt::UserRole).toString();
    if (roiId.isEmpty()) return;

    // 选中该项
    handleRoiSelection(roiId);

    // 获取 ROI 真实名称（item->text(0) 可能带有检测项后缀）
    RoiConfig* roi = m_roiManager.getRoiConfig(roiId);
    QString roiName = roi ? roi->roiName : item->text(0);

    // 构造菜单
    QMenu menu;
    QAction* renameAct = menu.addAction("重命名");
    menu.addSeparator();
    QAction* deleteAct = menu.addAction("删除");

    QAction* chosen = menu.exec(m_treeView->viewport()->mapToGlobal(pos));
    if (chosen == renameAct) {
        bool ok;
        QString newName = QInputDialog::getText(
            nullptr, "重命名ROI", "请输入新名称:",
            QLineEdit::Normal, roiName, &ok);
        if (ok && !newName.isEmpty() && newName != roiName) {
            renameRoi(roiId, newName);
        }
    } else if (chosen == deleteAct) {
        auto reply = QMessageBox::question(
            nullptr, "确认删除",
            QString("确定要删除ROI \"%1\" 吗？").arg(roiName),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
        if (reply == QMessageBox::Yes) {
            removeRoiAndRefresh(roiId);
        }
    }
}

// ========== ROI操作方法（供MainWindow委托调用）==========

QString RoiUiController::addRoiWithName(const QString& roiName)
{
    if (m_roiManager.getFullImage().empty()) {
        QMessageBox::warning(nullptr, "警告", "请先加载一张图像");
        return QString();
    }
    Logger::instance()->info(QString("请在图像上绘制新的ROI: %1").arg(roiName));
    enterRoiDrawMode();
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
        Logger::instance()->debug(QString("[RoiUI] 已保存ROI [%1] 的Pipeline配置: brightness=%2, contrast=%3, gamma=%4")
            .arg(roi->roiName)
            .arg(roi->pipelineConfig.enhance.brightness)
            .arg(roi->pipelineConfig.enhance.contrast)
            .arg(roi->pipelineConfig.enhance.gamma));
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
        m_pipelineManager->rebuildPipeline();
        
        emit roiPipelineConfigChanged(roi->pipelineConfig);
        
        Logger::instance()->info(QString("[RoiUI] 已加载ROI [%1] 的Pipeline配置: brightness=%2, contrast=%3, gamma=%4, sharpen=%5, channel=%6")
            .arg(roi->roiName)
            .arg(roi->pipelineConfig.enhance.brightness)
            .arg(roi->pipelineConfig.enhance.contrast)
            .arg(roi->pipelineConfig.enhance.gamma)
            .arg(roi->pipelineConfig.enhance.sharpen)
            .arg(static_cast<int>(roi->pipelineConfig.colorFilter.channel)));
    } else {
        Logger::instance()->warning(QString("[RoiUI] loadRoiPipelineConfig: ROI不存在, id=%1").arg(roiId));
    }
}

// ========== 图片切换时的ROI配置同步 ==========

void RoiUiController::syncRoiConfigsToWidget()
{
    refreshRoiTreeView();
    Logger::instance()->info("[RoiUiController] 已同步ROI配置到Widget");
}

// ========== 显示信号连接（从MainWindow迁移） ==========

void RoiUiController::setupDisplayConnections(TabManager* tabManager)
{
    // 纯显示切换：切换到ROI区域图像，无需Pipeline重新处理
    // 使用 setImageKeepZoom 保持当前缩放比例，避免切换时缩放重置
    connect(this, &RoiUiController::roiDisplayChanged,
            this, [this](const QString& roiId) {
        Q_UNUSED(roiId);
        cv::Mat currentImage = m_roiManager.getCurrentImage();
        if (!currentImage.empty()) {
            QImage qimg = ImageUtils::matToQImage(currentImage);
            m_view->setImageKeepZoom(qimg);
            if (m_statusBar) m_statusBar->showMessage("已切换显示", 2000);
        }
    });

    // 切换到原图显示，无需Pipeline重新处理
    // 使用 setImageKeepZoom 保持当前缩放比例，避免切换时缩放重置
    connect(this, &RoiUiController::fullImageDisplayRequested,
            this, [this]() {
        cv::Mat fullImage = m_roiManager.getFullImage();
        if (!fullImage.empty()) {
            QImage qimg = ImageUtils::matToQImage(fullImage);
            m_view->setImageKeepZoom(qimg);
            if (m_statusBar) m_statusBar->showMessage("已切换到原图", 2000);
        }
    });

    // ROI切换时更新EnhanceTabWidget的UI显示
    connect(this, &RoiUiController::roiPipelineConfigChanged,
            this, [tabManager](const PipelineConfig& config) {
        if (auto* enhanceTab = tabManager->getTabAs<EnhanceTabWidget>("增强")) {
            enhanceTab->setEnhanceConfig(
                config.enhance.brightness,
                config.enhance.contrast,
                config.enhance.gamma,
                config.enhance.sharpen
            );
        }
    });
}

void RoiUiController::updateBlobDetectionConfig(int minCount, int maxCount)
{
    QString roiId = getCurrentSelectedRoiId();
    RoiConfig* roi = m_roiManager.getRoiConfig(roiId);
    if (!roi) return;
    for (auto& detItem : roi->detectionItems) {
        if (detItem.type == DetectionType::Blob && detItem.enabled) {
            BlobAnalysisConfig blobConfig;
            blobConfig.fromJson(detItem.config);
            blobConfig.minBlobCount = minCount;
            blobConfig.maxBlobCount = maxCount;
            detItem.config = blobConfig.toJson();
            Logger::instance()->info(QString("[RoiUiController] 判定阈值已更新: min=%1, max=%2").arg(minCount).arg(maxCount));
            break;
        }
    }
}