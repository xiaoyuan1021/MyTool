#include "widgets/detection_config_widget.h"
#include "logger.h"
#include <QDebug>
#include <QMessageBox>
#include <QInputDialog>

// ========== DetectionItemCard 实现 ==========

DetectionItemCard::DetectionItemCard(const DetectionItem& item, QWidget* parent)
    : QWidget(parent)
    , m_item(item)
    , m_enabledCheckBox(nullptr)
    , m_typeLabel(nullptr)
    , m_deleteButton(nullptr)
    , m_configGroup(nullptr)
{
    initUI();
    initConnections();
}

DetectionItemCard::~DetectionItemCard()
{
}

void DetectionItemCard::updateItem(const DetectionItem& item)
{
    m_item = item;
    
    // 更新UI显示
    if (m_enabledCheckBox) {
        m_enabledCheckBox->setChecked(item.enabled);
    }
    
    if (m_typeLabel) {
        m_typeLabel->setText(detectionTypeToString(item.type));
    }
}

DetectionItem DetectionItemCard::getItemConfig() const
{
    return m_item;
}

void DetectionItemCard::onEnabledChanged(int state)
{
    m_item.enabled = (state == Qt::Checked);
    emit itemConfigChanged(m_item.itemId);
}

void DetectionItemCard::onDeleteClicked()
{
    emit deleteRequested(m_item.itemId);
}

void DetectionItemCard::initUI()
{
    // 创建主布局
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(5, 5, 5, 5);
    mainLayout->setSpacing(5);

    // 创建头部布局
    QHBoxLayout* headerLayout = new QHBoxLayout();
    
    // 启用复选框
    m_enabledCheckBox = new QCheckBox(this);
    m_enabledCheckBox->setChecked(m_item.enabled);
    
    // 类型标签
    m_typeLabel = new QLabel(detectionTypeToString(m_item.type), this);
    m_typeLabel->setStyleSheet("QLabel { font-weight: bold; color: #333; }");
    
    // 删除按钮
    m_deleteButton = new QPushButton("×", this);
    m_deleteButton->setFixedSize(24, 24);
    m_deleteButton->setStyleSheet(
        "QPushButton { "
        "background-color: #ff4757; "
        "color: white; "
        "border: none; "
        "border-radius: 12px; "
        "font-weight: bold; "
        "font-size: 14px; "
        "} "
        "QPushButton:hover { "
        "background-color: #ff6b7a; "
        "} "
        "QPushButton:pressed { "
        "background-color: #ff3742; "
        "}"
    );
    
    headerLayout->addWidget(m_enabledCheckBox);
    headerLayout->addWidget(m_typeLabel);
    headerLayout->addStretch();
    headerLayout->addWidget(m_deleteButton);

    // 创建配置组
    m_configGroup = new QGroupBox("配置参数", this);
    QVBoxLayout* configLayout = new QVBoxLayout(m_configGroup);
    
    // 添加一些示例配置控件
    QLabel* nameLabel = new QLabel(QString("名称: %1").arg(m_item.itemName), this);
    QLabel* descLabel = new QLabel(QString("描述: %1").arg(m_item.description.isEmpty() ? "无" : m_item.description), this);
    
    configLayout->addWidget(nameLabel);
    configLayout->addWidget(descLabel);

    // 添加到主布局
    mainLayout->addLayout(headerLayout);
    mainLayout->addWidget(m_configGroup);

    // 设置样式
    setStyleSheet(
        "DetectionItemCard { "
        "background-color: #f8f9fa; "
        "border: 1px solid #dee2e6; "
        "border-radius: 6px; "
        "margin: 2px; "
        "} "
        "DetectionItemCard:hover { "
        "border-color: #adb5bd; "
        "} "
        "QGroupBox { "
        "font-weight: bold; "
        "border: 1px solid #dee2e6; "
        "border-radius: 4px; "
        "margin-top: 10px; "
        "padding-top: 10px; "
        "} "
        "QGroupBox::title { "
        "subcontrol-origin: margin; "
        "left: 10px; "
        "padding: 0 5px; "
        "}"
    );

    setLayout(mainLayout);
}

void DetectionItemCard::initConnections()
{
    connect(m_enabledCheckBox, &QCheckBox::stateChanged, 
            this, &DetectionItemCard::onEnabledChanged);
    connect(m_deleteButton, &QPushButton::clicked, 
            this, &DetectionItemCard::onDeleteClicked);
}

// ========== DetectionConfigWidget 实现 ==========

DetectionConfigWidget::DetectionConfigWidget(QWidget* parent)
    : QWidget(parent)
    , m_currentRoi(nullptr)
    , m_titleLabel(nullptr)
    , m_currentRoiLabel(nullptr)
    , m_addDetectionButton(nullptr)
    , m_deleteDetectionButton(nullptr)
    , m_scrollArea(nullptr)
    , m_scrollContent(nullptr)
    , m_detectionListLayout(nullptr)
    , m_noDetectionLabel(nullptr)
    , m_tabWidget(nullptr)
{
    initUI();
    initConnections();
    
    Logger::instance()->info("检测项配置组件初始化完成");
}

DetectionConfigWidget::~DetectionConfigWidget()
{
}

// ========== 数据管理接口 ==========

void DetectionConfigWidget::setCurrentRoi(RoiConfig* roi)
{
    m_currentRoi = roi;
    refreshDetectionList();
}

void DetectionConfigWidget::clearCurrentConfig()
{
    m_currentRoi = nullptr;
    refreshDetectionList();
}

void DetectionConfigWidget::refreshDetectionList()
{
    // 清空现有卡片
    for (auto card : m_detectionCards) {
        m_detectionListLayout->removeWidget(card);
        delete card;
    }
    m_detectionCards.clear();

    // 更新当前ROI标签
    if (m_currentRoi) {
        m_currentRoiLabel->setText(QString("当前ROI: %1").arg(m_currentRoi->roiName));
        
        // 添加检测项卡片
        for (const auto& item : m_currentRoi->detectionItems) {
            DetectionItemCard* card = createDetectionCard(item);
            m_detectionListLayout->addWidget(card);
            m_detectionCards.append(card);
        }
    } else {
        m_currentRoiLabel->setText("当前ROI: 未选择");
    }

    // 更新无检测项提示
    updateNoDetectionLabel();
}

// ========== 私有槽函数 ==========

void DetectionConfigWidget::onAddDetectionClicked()
{
    if (!m_currentRoi) {
        QMessageBox::information(this, "提示", "请先选择一个ROI！");
        return;
    }

    // 创建检测类型选择对话框
    QStringList typeNames;
    typeNames << "条码检测" << "形状检测" << "颜色检测" << "纹理检测" 
              << "模板匹配" << "直线检测" << "圆形检测" << "Blob分析";

    bool ok;
    QString selectedType = QInputDialog::getItem(this, 
        "选择检测类型", 
        "请选择要添加的检测类型:", 
        typeNames, 
        0, 
        false, 
        &ok);

    if (!ok || selectedType.isEmpty()) {
        return;
    }

    // 转换为检测类型
    DetectionType type = stringToDetectionType(selectedType);

    // 生成检测项名称
    QString name = generateNewDetectionName(type);

    // 弹出名称输入对话框
    QString itemName = QInputDialog::getText(this, 
        "输入检测项名称", 
        "请输入检测项名称:", 
        QLineEdit::Normal, 
        name, 
        &ok);

    if (!ok || itemName.isEmpty()) {
        return;
    }

    emit detectionAddRequested(type, itemName);
}

void DetectionConfigWidget::onDeleteDetectionClicked()
{
    if (!m_currentRoi) {
        QMessageBox::information(this, "提示", "请先选择一个ROI！");
        return;
    }

    if (m_currentRoi->detectionItems.isEmpty()) {
        QMessageBox::information(this, "提示", "当前ROI没有检测项！");
        return;
    }

    // 创建检测项选择对话框
    QStringList itemNames;
    for (const auto& item : m_currentRoi->detectionItems) {
        itemNames << item.itemName;
    }

    bool ok;
    QString selectedItem = QInputDialog::getItem(this, 
        "选择要删除的检测项", 
        "请选择要删除的检测项:", 
        itemNames, 
        0, 
        false, 
        &ok);

    if (!ok || selectedItem.isEmpty()) {
        return;
    }

    // 查找对应的检测项ID
    QString itemId;
    for (const auto& item : m_currentRoi->detectionItems) {
        if (item.itemName == selectedItem) {
            itemId = item.itemId;
            break;
        }
    }

    if (!itemId.isEmpty()) {
        // 确认删除
        QMessageBox::StandardButton reply = QMessageBox::question(this,
            "确认删除",
            QString("确定要删除检测项 \"%1\" 吗？").arg(selectedItem),
            QMessageBox::Yes | QMessageBox::No,
            QMessageBox::No);

        if (reply == QMessageBox::Yes) {
            emit detectionDeleteRequested(itemId);
        }
    }
}

void DetectionConfigWidget::onDetectionItemChanged(const QString& itemId)
{
    emit detectionConfigChanged(itemId);
}

void DetectionConfigWidget::onDetectionItemDeleteRequested(const QString& itemId)
{
    emit detectionDeleteRequested(itemId);
}

// ========== 私有初始化函数 ==========

void DetectionConfigWidget::initUI()
{
    // 创建主布局
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(5, 5, 5, 5);
    mainLayout->setSpacing(5);

    // 创建标题和按钮布局
    QHBoxLayout* headerLayout = new QHBoxLayout();
    
    m_titleLabel = new QLabel("检测项配置", this);
    m_titleLabel->setStyleSheet("QLabel { font-weight: bold; font-size: 12px; }");
    
    m_addDetectionButton = new QPushButton("+ 添加检测项", this);
    m_addDetectionButton->setMinimumSize(80, 28);
    m_addDetectionButton->setStyleSheet("QPushButton { font-size: 11px; }");
    
    m_deleteDetectionButton = new QPushButton("- 删除检测项", this);
    m_deleteDetectionButton->setMinimumSize(80, 28);
    m_deleteDetectionButton->setStyleSheet("QPushButton { font-size: 11px; }");
    
    headerLayout->addWidget(m_titleLabel);
    headerLayout->addStretch();
    headerLayout->addWidget(m_addDetectionButton);
    headerLayout->addWidget(m_deleteDetectionButton);

    // 当前ROI标签
    m_currentRoiLabel = new QLabel("当前ROI: 未选择", this);
    m_currentRoiLabel->setStyleSheet(
        "QLabel { "
        "background-color: #f0f0f0; "
        "padding: 8px; "
        "border: 1px solid #ccc; "
        "border-radius: 4px; "
        "font-weight: bold; "
        "}"
    );

    // 创建滚动区域
    m_scrollArea = new QScrollArea(this);
    m_scrollArea->setWidgetResizable(true);
    m_scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);

    // 创建滚动内容
    m_scrollContent = new QWidget();
    m_detectionListLayout = new QVBoxLayout(m_scrollContent);
    m_detectionListLayout->setContentsMargins(5, 5, 5, 5);
    m_detectionListLayout->setSpacing(5);
    m_detectionListLayout->addStretch(); // 添加弹性空间

    m_scrollArea->setWidget(m_scrollContent);

    // 无检测项提示
    m_noDetectionLabel = new QLabel("请先选择一个ROI", this);
    m_noDetectionLabel->setAlignment(Qt::AlignCenter);
    m_noDetectionLabel->setStyleSheet(
        "QLabel { "
        "color: #999; "
        "font-style: italic; "
        "padding: 20px; "
        "}"
    );

    // 标签页控件
    m_tabWidget = new QTabWidget(this);
    m_tabWidget->setTabBarAutoHide(true);

    // 添加到主布局
    mainLayout->addLayout(headerLayout);
    mainLayout->addWidget(m_currentRoiLabel);
    mainLayout->addWidget(m_scrollArea);
    mainLayout->addWidget(m_noDetectionLabel);
    mainLayout->addWidget(m_tabWidget);

    setLayout(mainLayout);
}

void DetectionConfigWidget::initConnections()
{
    connect(m_addDetectionButton, &QPushButton::clicked, 
            this, &DetectionConfigWidget::onAddDetectionClicked);
    connect(m_deleteDetectionButton, &QPushButton::clicked, 
            this, &DetectionConfigWidget::onDeleteDetectionClicked);
}

// ========== 辅助函数 ==========

DetectionItemCard* DetectionConfigWidget::createDetectionCard(const DetectionItem& item)
{
    DetectionItemCard* card = new DetectionItemCard(item, this);
    
    connect(card, &DetectionItemCard::itemConfigChanged, 
            this, &DetectionConfigWidget::onDetectionItemChanged);
    connect(card, &DetectionItemCard::deleteRequested, 
            this, &DetectionConfigWidget::onDetectionItemDeleteRequested);
    
    return card;
}

void DetectionConfigWidget::updateNoDetectionLabel()
{
    if (!m_currentRoi) {
        m_noDetectionLabel->setText("请先选择一个ROI");
        m_noDetectionLabel->show();
        m_scrollArea->hide();
    } else if (m_currentRoi->detectionItems.isEmpty()) {
        m_noDetectionLabel->setText("当前ROI没有检测项\n点击\"+ 添加检测项\"来添加");
        m_noDetectionLabel->show();
        m_scrollArea->hide();
    } else {
        m_noDetectionLabel->hide();
        m_scrollArea->show();
    }
}

QString DetectionConfigWidget::generateNewDetectionName(DetectionType type) const
{
    QString baseName = detectionTypeToString(type);
    
    if (!m_currentRoi) {
        return baseName + "_1";
    }

    int maxIndex = 0;
    for (const auto& item : m_currentRoi->detectionItems) {
        if (item.itemName.startsWith(baseName)) {
            QString suffix = item.itemName.mid(baseName.length());
            if (suffix.startsWith("_")) {
                bool ok;
                int index = suffix.mid(1).toInt(&ok);
                if (ok && index > maxIndex) {
                    maxIndex = index;
                }
            }
        }
    }

    return QString("%1_%2").arg(baseName).arg(maxIndex + 1);
}

QWidget* DetectionConfigWidget::createDetectionTypeConfig(DetectionType type)
{
    QWidget* configWidget = new QWidget();
    QVBoxLayout* layout = new QVBoxLayout(configWidget);
    
    switch (type) {
        case DetectionType::Barcode: {
            QLabel* label = new QLabel("条码检测配置");
            QComboBox* formatCombo = new QComboBox();
            formatCombo->addItems({"QR Code", "Data Matrix", "Code 128", "Code 39"});
            layout->addWidget(label);
            layout->addWidget(new QLabel("条码格式:"));
            layout->addWidget(formatCombo);
            break;
        }
        case DetectionType::Shape: {
            QLabel* label = new QLabel("形状检测配置");
            QDoubleSpinBox* thresholdSpin = new QDoubleSpinBox();
            thresholdSpin->setRange(0.0, 1.0);
            thresholdSpin->setValue(0.8);
            layout->addWidget(label);
            layout->addWidget(new QLabel("匹配阈值:"));
            layout->addWidget(thresholdSpin);
            break;
        }
        case DetectionType::Color: {
            QLabel* label = new QLabel("颜色检测配置");
            QSpinBox* hueMinSpin = new QSpinBox();
            QSpinBox* hueMaxSpin = new QSpinBox();
            hueMinSpin->setRange(0, 180);
            hueMaxSpin->setRange(0, 180);
            layout->addWidget(label);
            layout->addWidget(new QLabel("色调范围:"));
            layout->addWidget(hueMinSpin);
            layout->addWidget(hueMaxSpin);
            break;
        }
        default: {
            QLabel* label = new QLabel(QString("%1 配置").arg(detectionTypeToString(type)));
            layout->addWidget(label);
            break;
        }
    }
    
    return configWidget;
}