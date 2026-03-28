#pragma once

#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QGroupBox>
#include <QCheckBox>
#include <QComboBox>
#include <QLineEdit>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QTabWidget>
#include <QStackedWidget>
#include "roi_config.h"
#include "widgets/blob_analysis_config_widget.h"
#include "widgets/line_detection_config_widget.h"
#include "widgets/barcode_recognition_config_widget.h"

/**
 * @brief 检测项配置卡片组件
 * 
 * 用于显示和配置单个检测项的参数
 */
class DetectionItemCard : public QWidget
{
    Q_OBJECT

public:
    explicit DetectionItemCard(const DetectionItem& item, QWidget* parent = nullptr);
    ~DetectionItemCard();

    /**
     * @brief 获取检测项ID
     */
    QString getItemId() const { return m_item.itemId; }

    /**
     * @brief 更新检测项数据
     */
    void updateItem(const DetectionItem& item);

    /**
     * @brief 获取当前检测项配置
     */
    DetectionItem getItemConfig() const;

signals:
    /**
     * @brief 检测项配置变化信号
     * @param itemId 检测项ID
     */
    void itemConfigChanged(const QString& itemId);

    /**
     * @brief 删除检测项请求信号
     * @param itemId 检测项ID
     */
    void deleteRequested(const QString& itemId);

private slots:
    void onEnabledChanged(int state);
    void onDeleteClicked();
    void onConfigChanged();

private:
    void initUI();
    void initConnections();
    void createConfigWidget();

    DetectionItem m_item;
    
    QCheckBox* m_enabledCheckBox;
    QLabel* m_typeLabel;
    QPushButton* m_deleteButton;
    QGroupBox* m_configGroup;
    QVBoxLayout* m_configLayout;
    
    // 配置组件（根据类型创建）
    BlobAnalysisConfigWidget* m_blobConfigWidget;
    LineDetectionConfigWidget* m_lineConfigWidget;
    BarcodeRecognitionConfigWidget* m_barcodeConfigWidget;
};

/**
 * @brief 检测项配置组件
 * 
 * 功能：
 * 1. 显示当前选中ROI的检测项列表
 * 2. 支持添加/删除检测项
 * 3. 支持配置检测项参数
 * 4. 动态更新UI
 */
class DetectionConfigWidget : public QWidget
{
    Q_OBJECT

public:
    explicit DetectionConfigWidget(QWidget* parent = nullptr);
    ~DetectionConfigWidget();

    // ========== 数据管理接口 ==========

    /**
     * @brief 设置当前ROI配置
     * @param roi ROI配置指针
     */
    void setCurrentRoi(RoiConfig* roi);

    /**
     * @brief 清空当前配置
     */
    void clearCurrentConfig();

    /**
     * @brief 刷新检测项列表
     */
    void refreshDetectionList();

signals:
    // ========== 信号 ==========

    /**
     * @brief 检测项添加信号
     * @param type 检测类型
     * @param name 检测项名称
     */
    void detectionAddRequested(DetectionType type, const QString& name);

    /**
     * @brief 检测项删除信号
     * @param itemId 检测项ID
     */
    void detectionDeleteRequested(const QString& itemId);

    /**
     * @brief 检测项配置变化信号
     * @param itemId 检测项ID
     */
    void detectionConfigChanged(const QString& itemId);

private slots:
    // ========== 槽函数 ==========

    /**
     * @brief 添加检测项按钮点击
     */
    void onAddDetectionClicked();

    /**
     * @brief 删除检测项按钮点击
     */
    void onDeleteDetectionClicked();

    /**
     * @brief 检测项卡片配置变化
     * @param itemId 检测项ID
     */
    void onDetectionItemChanged(const QString& itemId);

    /**
     * @brief 检测项卡片删除请求
     * @param itemId 检测项ID
     */
    void onDetectionItemDeleteRequested(const QString& itemId);

private:
    // ========== UI组件 ==========

    QLabel* m_titleLabel;               // 标题标签
    QLabel* m_currentRoiLabel;          // 当前ROI标签
    QPushButton* m_addDetectionButton;  // 添加检测项按钮
    QPushButton* m_deleteDetectionButton; // 删除检测项按钮
    QScrollArea* m_scrollArea;          // 滚动区域
    QWidget* m_scrollContent;           // 滚动内容
    QVBoxLayout* m_detectionListLayout; // 检测项列表布局
    QLabel* m_noDetectionLabel;         // 无检测项提示
    QTabWidget* m_tabWidget;            // 标签页控件

    // ========== 数据成员 ==========

    RoiConfig* m_currentRoi;            // 当前ROI配置
    QList<DetectionItemCard*> m_detectionCards; // 检测项卡片列表

    // ========== 初始化函数 ==========

    /**
     * @brief 初始化UI
     */
    void initUI();

    /**
     * @brief 初始化信号连接
     */
    void initConnections();

    // ========== 辅助函数 ==========

    /**
     * @brief 创建检测项卡片
     * @param item 检测项配置
     * @return 检测项卡片
     */
    DetectionItemCard* createDetectionCard(const DetectionItem& item);

    /**
     * @brief 更新无检测项提示
     */
    void updateNoDetectionLabel();

    /**
     * @brief 生成新的检测项名称
     * @param type 检测类型
     * @return 新检测项名称
     */
    QString generateNewDetectionName(DetectionType type) const;

    /**
     * @brief 获取检测类型对应的配置控件
     * @param type 检测类型
     * @return 配置控件
     */
    QWidget* createDetectionTypeConfig(DetectionType type);
};