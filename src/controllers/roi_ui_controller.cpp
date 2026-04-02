#include "controllers/roi_ui_controller.h"
#include "roi_config.h"
#include "logger.h"
#include "image_utils.h"

#include <QApplication>
#include <QDebug>

RoiUiController::RoiUiController(
    MultiRoiConfig* multiRoiConfig,
    RoiManager& roiManager,
    ImageView* imageView,
    QStatusBar* statusBar,
    QObject* parent)
    : QObject(parent)
    , m_multiRoiConfig(multiRoiConfig)
    , m_roiManager(roiManager)
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
    connect(m_treeView, &QTreeWidget::itemDoubleClicked,
            this, &RoiUiController::onRoiTreeItemDoubleClicked);
    connect(m_treeView, &QTreeWidget::customContextMenuRequested,
            this, &RoiUiController::onRoiTreeContextMenuRequested);
    
    // 连接绘制ROI完成信号
    connect(m_view, &ImageView::roiSelected, this, [this](const QRectF& rect) {
        // 创建新的ROI
        QString roiName = QString("ROI_%1").arg(m_multiRoiConfig->size() + 1);
        RoiConfig roi(roiName, rect);
        m_multiRoiConfig->addRoi(roi);
        
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
    RoiConfig* roi = m_multiRoiConfig->getRoi(roiId);
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
        m_multiRoiConfig->removeRoi(roiId);
        
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
        m_roiManager.resetRoi();
        m_view->clearRoi();
        m_view->resetZoom();
        m_view->setImage(ImageUtils::matToQImage(m_roiManager.getFullImage()));
        Logger::instance()->info("已切换到原图模式");
    } else {
        // 当前是原图模式，切换到ROI模式
        // 检查是否有选中的ROI
        if (m_currentSelectedRoiId.isEmpty()) {
            QMessageBox::warning(nullptr, "警告", "请先在左侧列表中选择一个ROI");
            return;
        }
        
        RoiConfig* roi = m_multiRoiConfig->getRoi(m_currentSelectedRoiId);
        if (!roi) {
            QMessageBox::warning(nullptr, "警告", "未找到选中的ROI");
            return;
        }
        
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
    
    // 保存当前选中的ROI ID，以便刷新后恢复选中状态
    QString previouslySelectedRoiId = m_currentSelectedRoiId;
    
    m_treeView->clear();
    
    const QList<RoiConfig>& rois = m_multiRoiConfig->getRois();
    
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
    
    // 恢复选中状态
    if (itemToSelect) {
        // 如果之前选中的ROI还存在，恢复选中状态
        m_treeView->setCurrentItem(itemToSelect);
        // 确保m_currentSelectedRoiId被正确设置
        m_currentSelectedRoiId = itemToSelect->data(0, Qt::UserRole).toString();
    } else if (m_treeView->topLevelItemCount() > 0) {
        // 如果之前选中的ROI不存在了（被删除了），选中最后一个ROI
        QTreeWidgetItem* lastItem = m_treeView->topLevelItem(
            m_treeView->topLevelItemCount() - 1
        );
        m_treeView->setCurrentItem(lastItem);
        m_currentSelectedRoiId = lastItem->data(0, Qt::UserRole).toString();
    } else {
        // 没有任何ROI
        m_currentSelectedRoiId.clear();
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
            m_currentSelectedRoiId = parentItem->data(0, Qt::UserRole).toString();
            
            // 发出检测项选中信号，用于触发Tab切换
            emit detectionItemSelected(m_currentSelectedRoiId, itemId);
        }
        
        Logger::instance()->info(QString("选中检测项: %1").arg(item->text(0)));
    } else {
        // 点击的是ROI - 只更新选中状态，不重新缩放图像
        m_currentSelectedRoiId = itemId;
        
        // 在图像视图中显示对应的ROI（不重置缩放）
        RoiConfig* roi = m_multiRoiConfig->getRoi(itemId);
        if (roi) {
            m_view->clearRoi();
            // 直接使用QRectF，无需转换
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
    
    if (!item) {
        return;
    }
}

void RoiUiController::onRoiTreeContextMenuRequested(const QPoint& pos)
{
    // 不再显示右键菜单，改用按钮实现删除功能
    Q_UNUSED(pos);
}