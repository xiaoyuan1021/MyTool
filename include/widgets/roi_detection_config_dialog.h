#pragma once

#include <QDialog>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QGroupBox>
#include <QLineEdit>
#include <QColorDialog>
#include <QList>
#include "roi_config.h"

/**
 * @brief ROI配置管理对话框
 * 
 * 功能：
 * 1. 显示和管理ROI列表
 * 2. 支持添加/删除ROI
 * 3. 支持配置ROI名称和颜色
 */
class RoiDetectionConfigDialog : public QDialog
{
    Q_OBJECT

public:
    /**
     * @brief 构造函数
     * @param rois 当前ROI列表
     * @param parent 父组件
     */
    explicit RoiDetectionConfigDialog(const QList<RoiConfig>& rois,
                                       QWidget* parent = nullptr);
    
    /**
     * @brief 获取配置后的ROI列表
     * @return ROI列表
     */
    QList<RoiConfig> getRoiConfigs() const { return m_roiConfigs; }

signals:
    /**
     * @brief ROI配置完成信号
     * @param rois 新的ROI列表
     */
    void roisConfigured(const QList<RoiConfig>& rois);

private slots:
    /**
     * @brief 添加ROI按钮点击
     */
    void onAddRoiClicked();
    
    /**
     * @brief 删除ROI按钮点击
     */
    void onDeleteRoiClicked();
    
    /**
     * @brief ROI选中变化
     * @param item 选中的树形项目
     * @param column 列号
     */
    void onRoiItemClicked(QTreeWidgetItem* item, int column);
    
    /**
     * @brief ROI名称编辑完成
     */
    void onRoiNameChanged();
    
    /**
     * @brief 选择颜色按钮点击
     */
    void onSelectColorClicked();
    
    /**
     * @brief 确定按钮点击
     */
    void onAcceptClicked();
    
    /**
     * @brief 取消按钮点击
     */
    void onRejectClicked();

private:
    // ========== UI组件 ==========
    QLabel* m_titleLabel;                   // 标题标签
    QTreeWidget* m_roiTreeWidget;           // ROI树形控件
    QPushButton* m_addButton;               // 添加按钮
    QPushButton* m_deleteButton;            // 删除按钮
    
    // ROI配置区域
    QGroupBox* m_configGroupBox;            // 配置组框
    QLineEdit* m_roiNameEdit;               // ROI名称编辑框
    QPushButton* m_colorButton;             // 颜色选择按钮
    QLabel* m_colorPreview;                 // 颜色预览标签
    
    // 按钮
    QPushButton* m_okButton;
    QPushButton* m_cancelButton;
    
    // ========== 数据成员 ==========
    QList<RoiConfig> m_roiConfigs;          // ROI配置列表
    int m_currentSelectedIndex;             // 当前选中的ROI索引
    int m_nextRoiNumber;                    // 下一个ROI编号
    QColor m_currentColor;                  // 当前选中的颜色
    
    // ========== 初始化函数 ==========
    void initUI();
    void initConnections();
    
    // ========== 辅助函数 ==========
    /**
     * @brief 创建ROI树形项目
     * @param roi ROI配置
     * @return 树形项目
     */
    QTreeWidgetItem* createRoiTreeWidgetItem(const RoiConfig& roi);
    
    /**
     * @brief 更新树形项目显示
     * @param treeItem 树形项目
     * @param roi ROI配置
     */
    void updateRoiTreeWidgetItem(QTreeWidgetItem* treeItem, const RoiConfig& roi);
    
    /**
     * @brief 生成新的ROI名称
     * @return 新名称
     */
    QString generateRoiName();
    
    /**
     * @brief 更新配置区域显示
     * @param index ROI索引
     */
    void updateConfigWidget(int index);
    
    /**
     * @brief 从UI控件保存配置到ROI
     * @param index ROI索引
     */
    void saveConfigFromUI(int index);
    
    /**
     * @brief 更新颜色按钮显示
     * @param color 颜色
     */
    void updateColorButton(const QColor& color);
};
