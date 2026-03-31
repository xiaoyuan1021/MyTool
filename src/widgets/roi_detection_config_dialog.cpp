#include "roi_detection_config_dialog.h"
#include <QMessageBox>
#include <QDebug>
#include <QIcon>

RoiDetectionConfigDialog::RoiDetectionConfigDialog(const QList<RoiConfig>& rois,
                                                     QWidget* parent)
    : QDialog(parent)
    , m_roiConfigs(rois)
    , m_currentSelectedIndex(-1)
    , m_nextRoiNumber(1)
    , m_currentColor(Qt::red)
{
    // 计算下一个编号
    for (const auto& roi : m_roiConfigs) {
        // 从名称中提取编号
        QString name = roi.roiName;
        QStringList parts = name.split("_");
        if (parts.size() >= 2) {
            bool ok;
            int num = parts.last().toInt(&ok);
            if (ok && num >= m_nextRoiNumber) {
                m_nextRoiNumber = num + 1;
            }
        }
    }
    
    initUI();
    initConnections();
    
    // 刷新列表显示
    for (const auto& roi : m_roiConfigs) {
        QTreeWidgetItem* treeItem = createRoiTreeWidgetItem(roi);
        m_roiTreeWidget->addTopLevelItem(treeItem);
    }
    
    // 如果有ROI，选中第一个
    if (m_roiTreeWidget->topLevelItemCount() > 0) {
        m_roiTreeWidget->setCurrentItem(m_roiTreeWidget->topLevelItem(0));
        onRoiItemClicked(m_roiTreeWidget->topLevelItem(0), 0);
    }
}

void RoiDetectionConfigDialog::initUI()
{
    setWindowTitle("ROI配置管理");
    setMinimumSize(500, 400);
    
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    
    // 标题
    m_titleLabel = new QLabel("ROI配置管理");
    m_titleLabel->setStyleSheet("font-size: 14px; font-weight: bold; padding: 5px;");
    mainLayout->addWidget(m_titleLabel);
    
    // 水平布局：左侧列表，右侧配置
    QHBoxLayout* contentLayout = new QHBoxLayout();
    
    // 左侧：ROI列表
    QVBoxLayout* leftLayout = new QVBoxLayout();
    QLabel* listLabel = new QLabel("ROI列表:");
    leftLayout->addWidget(listLabel);
    
    m_roiTreeWidget = new QTreeWidget();
    m_roiTreeWidget->setHeaderLabels({"名称", "检测项数"});
    m_roiTreeWidget->setColumnWidth(0, 150);
    leftLayout->addWidget(m_roiTreeWidget);
    
    // 添加/删除按钮
    QHBoxLayout* buttonLayout = new QHBoxLayout();
    m_addButton = new QPushButton("添加ROI");
    m_deleteButton = new QPushButton("删除ROI");
    buttonLayout->addWidget(m_addButton);
    buttonLayout->addWidget(m_deleteButton);
    leftLayout->addLayout(buttonLayout);
    
    contentLayout->addLayout(leftLayout, 1);
    
    // 右侧：ROI配置
    QVBoxLayout* rightLayout = new QVBoxLayout();
    m_configGroupBox = new QGroupBox("ROI配置");
    QVBoxLayout* configLayout = new QVBoxLayout(m_configGroupBox);
    
    // ROI名称
    QHBoxLayout* nameLayout = new QHBoxLayout();
    nameLayout->addWidget(new QLabel("名称:"));
    m_roiNameEdit = new QLineEdit();
    m_roiNameEdit->setPlaceholderText("输入ROI名称");
    nameLayout->addWidget(m_roiNameEdit);
    configLayout->addLayout(nameLayout);
    
    // ROI颜色
    QHBoxLayout* colorLayout = new QHBoxLayout();
    colorLayout->addWidget(new QLabel("颜色:"));
    m_colorPreview = new QLabel();
    m_colorPreview->setFixedSize(24, 24);
    m_colorPreview->setStyleSheet("background-color: red; border: 1px solid #999;");
    colorLayout->addWidget(m_colorPreview);
    m_colorButton = new QPushButton("选择颜色");
    colorLayout->addWidget(m_colorButton);
    colorLayout->addStretch();
    configLayout->addLayout(colorLayout);
    
    configLayout->addStretch();
    rightLayout->addWidget(m_configGroupBox);
    
    contentLayout->addLayout(rightLayout, 1);
    
    mainLayout->addLayout(contentLayout);
    
    // 底部按钮
    QHBoxLayout* bottomLayout = new QHBoxLayout();
    bottomLayout->addStretch();
    m_okButton = new QPushButton("确定");
    m_cancelButton = new QPushButton("取消");
    m_okButton->setMinimumWidth(80);
    m_cancelButton->setMinimumWidth(80);
    bottomLayout->addWidget(m_okButton);
    bottomLayout->addWidget(m_cancelButton);
    
    mainLayout->addLayout(bottomLayout);
}

void RoiDetectionConfigDialog::initConnections()
{
    connect(m_addButton, &QPushButton::clicked, this, &RoiDetectionConfigDialog::onAddRoiClicked);
    connect(m_deleteButton, &QPushButton::clicked, this, &RoiDetectionConfigDialog::onDeleteRoiClicked);
    connect(m_roiTreeWidget, &QTreeWidget::itemClicked, this, &RoiDetectionConfigDialog::onRoiItemClicked);
    connect(m_roiNameEdit, &QLineEdit::editingFinished, this, &RoiDetectionConfigDialog::onRoiNameChanged);
    connect(m_colorButton, &QPushButton::clicked, this, &RoiDetectionConfigDialog::onSelectColorClicked);
    connect(m_okButton, &QPushButton::clicked, this, &RoiDetectionConfigDialog::onAcceptClicked);
    connect(m_cancelButton, &QPushButton::clicked, this, &RoiDetectionConfigDialog::onRejectClicked);
}

QTreeWidgetItem* RoiDetectionConfigDialog::createRoiTreeWidgetItem(const RoiConfig& roi)
{
    QTreeWidgetItem* treeItem = new QTreeWidgetItem();
    updateRoiTreeWidgetItem(treeItem, roi);
    return treeItem;
}

void RoiDetectionConfigDialog::updateRoiTreeWidgetItem(QTreeWidgetItem* treeItem, const RoiConfig& roi)
{
    treeItem->setText(0, roi.roiName);
    treeItem->setText(1, QString("%1个").arg(roi.detectionItems.size()));
    
    // 设置颜色
    QColor color(roi.color);
    treeItem->setForeground(0, QBrush(color));
}

QString RoiDetectionConfigDialog::generateRoiName()
{
    return QString("ROI_%1").arg(m_nextRoiNumber++);
}

void RoiDetectionConfigDialog::onAddRoiClicked()
{
    // 创建新的ROI
    QString name = generateRoiName();
    RoiConfig newRoi;              // 使用默认构造函数
    newRoi.roiName = name;         // 设置名称
    newRoi.color = m_currentColor.name();  // 设置颜色
    
    // 添加到列表
    m_roiConfigs.append(newRoi);
    
    // 添加到树形控件
    QTreeWidgetItem* treeItem = createRoiTreeWidgetItem(newRoi);
    m_roiTreeWidget->addTopLevelItem(treeItem);
    
    // 选中新添加的项
    m_roiTreeWidget->setCurrentItem(treeItem);
    onRoiItemClicked(treeItem, 0);
    
    // 聚焦到名称编辑框，方便用户修改名称
    m_roiNameEdit->setFocus();
    m_roiNameEdit->selectAll();
    
    qDebug() << "添加ROI:" << name;
}

void RoiDetectionConfigDialog::onDeleteRoiClicked()
{
    QTreeWidgetItem* currentItem = m_roiTreeWidget->currentItem();
    if (!currentItem) {
        QMessageBox::warning(this, "警告", "请先选择要删除的ROI");
        return;
    }
    
    int index = m_roiTreeWidget->indexOfTopLevelItem(currentItem);
    if (index < 0 || index >= m_roiConfigs.size()) {
        return;
    }
    
    // 确认删除
    QString name = m_roiConfigs[index].roiName;
    QMessageBox::StandardButton reply = QMessageBox::question(
        this, "确认删除",
        QString("确定要删除ROI \"%1\" 吗?").arg(name),
        QMessageBox::Yes | QMessageBox::No
    );
    
    if (reply == QMessageBox::Yes) {
        // 从列表中删除
        m_roiConfigs.removeAt(index);
        
        // 从树形控件中删除
        delete m_roiTreeWidget->takeTopLevelItem(index);
        
        // 更新选中状态
        m_currentSelectedIndex = -1;
        if (m_roiTreeWidget->topLevelItemCount() > 0) {
            int newIndex = qMin(index, m_roiTreeWidget->topLevelItemCount() - 1);
            QTreeWidgetItem* newItem = m_roiTreeWidget->topLevelItem(newIndex);
            m_roiTreeWidget->setCurrentItem(newItem);
            onRoiItemClicked(newItem, 0);
        } else {
            // 没有ROI时清空配置区域
            m_roiNameEdit->clear();
            updateColorButton(m_currentColor);
        }
        
        qDebug() << "删除ROI:" << name;
    }
}

void RoiDetectionConfigDialog::onRoiItemClicked(QTreeWidgetItem* item, int column)
{
    Q_UNUSED(column);
    
    int index = m_roiTreeWidget->indexOfTopLevelItem(item);
    if (index < 0 || index >= m_roiConfigs.size()) {
        return;
    }
    
    // 保存之前选中项的配置
    if (m_currentSelectedIndex >= 0 && m_currentSelectedIndex < m_roiConfigs.size()) {
        saveConfigFromUI(m_currentSelectedIndex);
    }
    
    m_currentSelectedIndex = index;
    updateConfigWidget(index);
}

void RoiDetectionConfigDialog::updateConfigWidget(int index)
{
    if (index < 0 || index >= m_roiConfigs.size()) {
        return;
    }
    
    const RoiConfig& roi = m_roiConfigs[index];
    
    // 更新名称
    m_roiNameEdit->setText(roi.roiName);
    
    // 更新颜色
    m_currentColor = QColor(roi.color);
    updateColorButton(m_currentColor);
}

void RoiDetectionConfigDialog::saveConfigFromUI(int index)
{
    if (index < 0 || index >= m_roiConfigs.size()) {
        return;
    }
    
    RoiConfig& roi = m_roiConfigs[index];
    
    // 保存名称
    QString newName = m_roiNameEdit->text().trimmed();
    if (!newName.isEmpty()) {
        roi.roiName = newName;
    }
    
    // 保存颜色
    roi.color = m_currentColor.name();
    
    // 更新树形控件显示
    QTreeWidgetItem* item = m_roiTreeWidget->topLevelItem(index);
    if (item) {
        updateRoiTreeWidgetItem(item, roi);
    }
}

void RoiDetectionConfigDialog::onRoiNameChanged()
{
    if (m_currentSelectedIndex >= 0 && m_currentSelectedIndex < m_roiConfigs.size()) {
        saveConfigFromUI(m_currentSelectedIndex);
    }
}

void RoiDetectionConfigDialog::onSelectColorClicked()
{
    QColor color = QColorDialog::getColor(m_currentColor, this, "选择ROI颜色");
    if (color.isValid()) {
        m_currentColor = color;
        updateColorButton(color);
        
        // 立即保存到当前选中的ROI
        if (m_currentSelectedIndex >= 0 && m_currentSelectedIndex < m_roiConfigs.size()) {
            saveConfigFromUI(m_currentSelectedIndex);
        }
    }
}

void RoiDetectionConfigDialog::updateColorButton(const QColor& color)
{
    m_colorPreview->setStyleSheet(
        QString("background-color: %1; border: 1px solid #999;").arg(color.name())
    );
}

void RoiDetectionConfigDialog::onAcceptClicked()
{
    // 保存当前选中项的配置
    if (m_currentSelectedIndex >= 0 && m_currentSelectedIndex < m_roiConfigs.size()) {
        saveConfigFromUI(m_currentSelectedIndex);
    }
    
    // 发送配置完成信号
    emit roisConfigured(m_roiConfigs);
    
    accept();
}

void RoiDetectionConfigDialog::onRejectClicked()
{
    reject();
}