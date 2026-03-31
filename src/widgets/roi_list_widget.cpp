#include "widgets/roi_list_widget.h"
#include "widgets/roi_detection_config_dialog.h"
#include "logger.h"
#include <QDebug>

RoiListWidget::RoiListWidget(QWidget* parent)
    : QWidget(parent)
    , m_roiConfig(nullptr)
    , m_titleLabel(nullptr)
    , m_addRoiButton(nullptr)
    , m_deleteRoiButton(nullptr)
    , m_drawRoiButton(nullptr)
    , m_configDetectionButton(nullptr)
    , m_roiTreeWidget(nullptr)
{
    initUI();
    initConnections();
    initContextMenu();
    
    Logger::instance()->info("ROI列表组件初始化完成");
}

RoiListWidget::~RoiListWidget()
{
}

// ========== 数据管理接口 ==========

void RoiListWidget::setRoiConfig(MultiRoiConfig* config)
{
    m_roiConfig = config;
    refreshRoiList();
}

void RoiListWidget::refreshRoiList()
{
    if (!m_roiTreeWidget) {
        return;
    }

    // 清空现有项目
    m_roiTreeWidget->clear();

    if (!m_roiConfig || m_roiConfig->isEmpty()) {
        // 如果没有ROI，显示提示信息
        QTreeWidgetItem* emptyItem = new QTreeWidgetItem(m_roiTreeWidget);
        emptyItem->setText(0, "暂无ROI");
        emptyItem->setFlags(emptyItem->flags() & ~Qt::ItemIsSelectable);
        emptyItem->setForeground(0, QBrush(QColor(150, 150, 150)));
        return;
    }

    // 添加ROI项目
    for (const auto& roi : m_roiConfig->getRois()) {
        QTreeWidgetItem* item = createRoiTreeWidgetItem(roi);
        m_roiTreeWidget->addTopLevelItem(item);
        
        // 如果是选中的ROI，设置选中状态
        if (roi.roiId == m_selectedRoiId) {
            item->setSelected(true);
        }
    }

    // 展开所有项目
    m_roiTreeWidget->expandAll();
}

void RoiListWidget::selectRoi(const QString& roiId)
{
    if (m_selectedRoiId == roiId) {
        return;
    }

    updateSelection(roiId);
    emit roiSelectionChanged(roiId);
}

QString RoiListWidget::getSelectedRoiId() const
{
    return m_selectedRoiId;
}

RoiConfig* RoiListWidget::getSelectedRoi()
{
    if (!m_roiConfig || m_selectedRoiId.isEmpty()) {
        return nullptr;
    }

    return m_roiConfig->getRoi(m_selectedRoiId);
}

// ========== 私有槽函数 ==========

void RoiListWidget::onAddRoiClicked()
{
    QString roiName = generateNewRoiName();
    
    // 弹出输入对话框让用户确认或修改名称
    bool ok;
    QString name = QInputDialog::getText(this, 
        "添加ROI", 
        "请输入ROI名称:", 
        QLineEdit::Normal, 
        roiName, 
        &ok);

    if (!ok || name.isEmpty()) {
        return;
    }

    // 验证名称
    if (!validateRoiName(name)) {
        QMessageBox::warning(this, "警告", "ROI名称无效或已存在！");
        return;
    }

    emit roiAddRequested(name);
}

void RoiListWidget::onDeleteRoiClicked()
{
    if (m_selectedRoiId.isEmpty()) {
        QMessageBox::information(this, "提示", "请先选择要删除的ROI！");
        return;
    }

    // 获取ROI名称用于确认提示
    RoiConfig* roi = getSelectedRoi();
    QString roiName = roi ? roi->roiName : m_selectedRoiId;

    // 确认删除
    QMessageBox::StandardButton reply = QMessageBox::question(this,
        "确认删除",
        QString("确定要删除ROI \"%1\" 吗？\n此操作不可恢复！").arg(roiName),
        QMessageBox::Yes | QMessageBox::No,
        QMessageBox::No);

    if (reply == QMessageBox::Yes) {
        emit roiDeleteRequested(m_selectedRoiId);
    }
}

void RoiListWidget::onRoiItemClicked(QTreeWidgetItem* item, int column)
{
    Q_UNUSED(column)

    if (!item) {
        return;
    }

    // 检查是否是有效ROI项目（不是提示信息）
    QString roiId = item->data(0, Qt::UserRole).toString();
    if (roiId.isEmpty()) {
        return;
    }

    if (roiId != m_selectedRoiId) {
        updateSelection(roiId);
        emit roiSelectionChanged(roiId);
    }
}

void RoiListWidget::onRoiContextMenuRequested(const QPoint& pos)
{
    QTreeWidgetItem* item = m_roiTreeWidget->itemAt(pos);
    if (!item) {
        return;
    }

    QString roiId = item->data(0, Qt::UserRole).toString();
    if (roiId.isEmpty()) {
        return;
    }

    // 选中右键点击的项目
    if (roiId != m_selectedRoiId) {
        updateSelection(roiId);
        emit roiSelectionChanged(roiId);
    }

    // 创建右键菜单
    QMenu contextMenu(this);
    
    QAction* renameAction = contextMenu.addAction("重命名");
    QAction* toggleActiveAction = contextMenu.addAction("激活/停用");
    contextMenu.addSeparator();
    QAction* deleteAction = contextMenu.addAction("删除");

    // 连接动作
    connect(renameAction, &QAction::triggered, [this, roiId]() {
        RoiConfig* roi = m_roiConfig->getRoi(roiId);
        if (!roi) return;

        bool ok;
        QString newName = QInputDialog::getText(this, 
            "重命名ROI", 
            "请输入新名称:", 
            QLineEdit::Normal, 
            roi->roiName, 
            &ok);

        if (ok && !newName.isEmpty() && newName != roi->roiName) {
            if (validateRoiName(newName)) {
                emit roiRenameRequested(roiId, newName);
            } else {
                QMessageBox::warning(this, "警告", "ROI名称无效或已存在！");
            }
        }
    });

    connect(toggleActiveAction, &QAction::triggered, [this, roiId]() {
        RoiConfig* roi = m_roiConfig->getRoi(roiId);
        if (roi) {
            emit roiActiveChanged(roiId, !roi->isActive);
        }
    });

    connect(deleteAction, &QAction::triggered, [this, roiId]() {
        RoiConfig* roi = m_roiConfig->getRoi(roiId);
        QString roiName = roi ? roi->roiName : roiId;

        QMessageBox::StandardButton reply = QMessageBox::question(this,
            "确认删除",
            QString("确定要删除ROI \"%1\" 吗？").arg(roiName),
            QMessageBox::Yes | QMessageBox::No,
            QMessageBox::No);

        if (reply == QMessageBox::Yes) {
            emit roiDeleteRequested(roiId);
        }
    });

    // 显示右键菜单
    contextMenu.exec(m_roiTreeWidget->viewport()->mapToGlobal(pos));
}

void RoiListWidget::onRoiItemDoubleClicked(QTreeWidgetItem* item, int column)
{
    Q_UNUSED(column)

    if (!item) {
        return;
    }

    QString roiId = item->data(0, Qt::UserRole).toString();
    if (roiId.isEmpty()) {
        return;
    }

    // 双击时触发重命名
    RoiConfig* roi = m_roiConfig->getRoi(roiId);
    if (!roi) return;

    bool ok;
    QString newName = QInputDialog::getText(this, 
        "重命名ROI", 
        "请输入新名称:", 
        QLineEdit::Normal, 
        roi->roiName, 
        &ok);

    if (ok && !newName.isEmpty() && newName != roi->roiName) {
        if (validateRoiName(newName)) {
            emit roiRenameRequested(roiId, newName);
        } else {
            QMessageBox::warning(this, "警告", "ROI名称无效或已存在！");
        }
    }
}

// ========== 私有初始化函数 ==========

void RoiListWidget::initUI()
{
    // 创建主布局
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(5, 5, 5, 5);
    mainLayout->setSpacing(5);

    // 创建标题和按钮布局
    QHBoxLayout* headerLayout = new QHBoxLayout();
    
    m_titleLabel = new QLabel("ROI列表", this);
    m_titleLabel->setStyleSheet("QLabel { font-weight: bold; font-size: 12px; }");
    
    m_drawRoiButton = new QPushButton("绘制ROI", this);
    m_drawRoiButton->setMinimumSize(60, 28);
    m_drawRoiButton->setStyleSheet("QPushButton { font-size: 11px; }");
    
    headerLayout->addWidget(m_titleLabel);
    headerLayout->addStretch();
    headerLayout->addWidget(m_drawRoiButton);

    // 创建操作按钮布局
    QHBoxLayout* buttonLayout = new QHBoxLayout();
    
    m_addRoiButton = new QPushButton("+ 添加ROI", this);
    m_addRoiButton->setMinimumSize(0, 30);
    
    m_deleteRoiButton = new QPushButton("- 删除ROI", this);
    m_deleteRoiButton->setMinimumSize(0, 30);
    
    m_configDetectionButton = new QPushButton("配置检测项", this);
    m_configDetectionButton->setMinimumSize(0, 30);
    
    buttonLayout->addWidget(m_addRoiButton);
    buttonLayout->addWidget(m_deleteRoiButton);
    buttonLayout->addWidget(m_configDetectionButton);

    // 创建树形控件
    m_roiTreeWidget = new QTreeWidget(this);
    m_roiTreeWidget->setHeaderHidden(true);
    m_roiTreeWidget->setRootIsDecorated(true);
    m_roiTreeWidget->setAlternatingRowColors(true);
    m_roiTreeWidget->setSelectionMode(QAbstractItemView::SingleSelection);
    m_roiTreeWidget->setSelectionBehavior(QAbstractItemView::SelectRows);

    // 添加到主布局
    mainLayout->addLayout(headerLayout);
    mainLayout->addLayout(buttonLayout);
    mainLayout->addWidget(m_roiTreeWidget);

    // 设置布局
    setLayout(mainLayout);
}

void RoiListWidget::initConnections()
{
    // 连接按钮信号
    connect(m_addRoiButton, &QPushButton::clicked, 
            this, &RoiListWidget::onAddRoiClicked);
    connect(m_deleteRoiButton, &QPushButton::clicked, 
            this, &RoiListWidget::onDeleteRoiClicked);
    connect(m_drawRoiButton, &QPushButton::clicked, 
            this, &RoiListWidget::drawRoiRequested);
    connect(m_configDetectionButton, &QPushButton::clicked, 
            this, &RoiListWidget::onConfigureDetectionClicked);

    // 连接树形控件信号
    connect(m_roiTreeWidget, &QTreeWidget::itemClicked, 
            this, &RoiListWidget::onRoiItemClicked);
    connect(m_roiTreeWidget, &QTreeWidget::itemDoubleClicked, 
            this, &RoiListWidget::onRoiItemDoubleClicked);
    connect(m_roiTreeWidget, &QTreeWidget::customContextMenuRequested, 
            this, &RoiListWidget::onRoiContextMenuRequested);

    // 启用右键菜单
    m_roiTreeWidget->setContextMenuPolicy(Qt::CustomContextMenu);
}

void RoiListWidget::initContextMenu()
{
    // 右键菜单在onRoiContextMenuRequested中动态创建
}

// ========== 辅助函数 ==========

QTreeWidgetItem* RoiListWidget::createRoiTreeWidgetItem(const RoiConfig& roi)
{
    QTreeWidgetItem* item = new QTreeWidgetItem();
    updateRoiTreeWidgetItem(item, roi);
    return item;
}

void RoiListWidget::updateRoiTreeWidgetItem(QTreeWidgetItem* item, const RoiConfig& roi)
{
    if (!item) {
        return;
    }

    // 设置显示文本
    QString displayText = roi.roiName;
    if (!roi.isActive) {
        displayText += " (已停用)";
    }
    
    // 显示检测项数量
    int detectionCount = roi.detectionItems.size();
    if (detectionCount > 0) {
        displayText += QString(" [%1个检测项]").arg(detectionCount);
    }

    item->setText(0, displayText);
    item->setData(0, Qt::UserRole, roi.roiId);

    // 设置颜色
    QColor roiColor(roi.color);
    if (!roi.isActive) {
        roiColor.setAlpha(100); // 停用的ROI半透明
    }
    item->setForeground(0, QBrush(roiColor));

    // 设置图标（如果有）
    // item->setIcon(0, QIcon(":/icons/roi.png"));

    // 添加检测项子节点
    for (const auto& detection : roi.detectionItems) {
        QTreeWidgetItem* detectionItem = new QTreeWidgetItem(item);
        QString detectionText = detection.itemName;
        if (!detection.enabled) {
            detectionText += " (已禁用)";
        }
        detectionItem->setText(0, detectionText);
        detectionItem->setData(0, Qt::UserRole, detection.itemId);
        
        // 检测项颜色
        QColor detectionColor = detection.enabled ? QColor(70, 130, 180) : QColor(150, 150, 150);
        detectionItem->setForeground(0, QBrush(detectionColor));
    }
}

QTreeWidgetItem* RoiListWidget::findRoiItem(const QString& roiId)
{
    if (roiId.isEmpty()) {
        return nullptr;
    }

    for (int i = 0; i < m_roiTreeWidget->topLevelItemCount(); ++i) {
        QTreeWidgetItem* item = m_roiTreeWidget->topLevelItem(i);
        if (item && item->data(0, Qt::UserRole).toString() == roiId) {
            return item;
        }
    }

    return nullptr;
}

void RoiListWidget::updateSelection(const QString& roiId)
{
    // 取消之前的选中状态
    if (!m_selectedRoiId.isEmpty()) {
        QTreeWidgetItem* oldItem = findRoiItem(m_selectedRoiId);
        if (oldItem) {
            oldItem->setSelected(false);
        }
    }

    // 更新选中的ROI ID
    m_selectedRoiId = roiId;

    // 设置新的选中状态
    if (!roiId.isEmpty()) {
        QTreeWidgetItem* newItem = findRoiItem(roiId);
        if (newItem) {
            newItem->setSelected(true);
            m_roiTreeWidget->scrollToItem(newItem);
        }
    }
}

QString RoiListWidget::generateNewRoiName() const
{
    if (!m_roiConfig) {
        return "ROI_1";
    }

    int maxIndex = 0;
    for (const auto& roi : m_roiConfig->getRois()) {
        QString name = roi.roiName;
        if (name.startsWith("ROI_")) {
            bool ok;
            int index = name.mid(4).toInt(&ok);
            if (ok && index > maxIndex) {
                maxIndex = index;
            }
        }
    }

    return QString("ROI_%1").arg(maxIndex + 1);
}

bool RoiListWidget::validateRoiName(const QString& name) const
{
    // 检查名称是否为空
    if (name.isEmpty()) {
        return false;
    }

    // 检查名称是否包含非法字符
    if (name.contains('/') || name.contains('\\') || name.contains(':')) {
        return false;
    }

    // 检查名称是否已存在
    if (m_roiConfig) {
        for (const auto& roi : m_roiConfig->getRois()) {
            if (roi.roiName == name) {
                return false;
            }
        }
    }

    return true;
}

void RoiListWidget::onConfigureDetectionClicked()
{
    if (m_selectedRoiId.isEmpty()) {
        QMessageBox::information(this, "提示", "请先选择要配置检测项的ROI！");
        return;
    }

    emit roiDetectionConfigRequested(m_selectedRoiId);
}
