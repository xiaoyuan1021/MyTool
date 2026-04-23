#pragma once

#include <QWidget>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QMenu>
#include <QAction>
#include <QMessageBox>
#include <QInputDialog>
#include "roi_config.h"
#include "ui/roi_manager.h"

/**
 * @brief ROI列表管理组件
 * 
 * 功能：
 * 1. 显示ROI列表（树形结构）
 * 2. 支持添加/删除ROI
 * 3. 支持ROI选中和高亮
 * 4. 支持右键菜单操作
 * 5. 显示每个ROI的检测项数量
 */
class RoiListWidget : public QWidget
{
    Q_OBJECT

public:
    explicit RoiListWidget(QWidget* parent = nullptr);
    ~RoiListWidget();

    // ========== 数据管理接口 ==========

    /**
     * @brief 设置ROI管理器引用
     * @param roiManager ROI管理器引用
     */
    void setRoiManager(RoiManager& roiManager);

    /**
     * @brief 刷新ROI列表显示
     */
    void refreshRoiList();

    /**
     * @brief 选中指定ROI
     * @param roiId ROI ID
     */
    void selectRoi(const QString& roiId);

    /**
     * @brief 获取当前选中的ROI ID
     * @return ROI ID，如果没有选中返回空字符串
     */
    QString getSelectedRoiId() const;

    /**
     * @brief 获取当前选中的ROI配置
     * @return ROI配置指针，如果没有选中返回nullptr
     */
    RoiConfig* getSelectedRoi();

signals:
    // ========== 信号 ==========

    /**
     * @brief ROI添加信号
     * @param roiName 新ROI名称
     */
    void roiAddRequested(const QString& roiName);

    /**
     * @brief ROI删除信号
     * @param roiId 要删除的ROI ID
     */
    void roiDeleteRequested(const QString& roiId);

    /**
     * @brief ROI选中变化信号
     * @param roiId 选中的ROI ID
     */
    void roiSelectionChanged(const QString& roiId);

    /**
     * @brief ROI重命名信号
     * @param roiId ROI ID
     * @param newName 新名称
     */
    void roiRenameRequested(const QString& roiId, const QString& newName);

    /**
     * @brief ROI激活状态变化信号
     * @param roiId ROI ID
     * @param active 激活状态
     */
    void roiActiveChanged(const QString& roiId, bool active);

    /**
     * @brief 绘制ROI请求信号
     */
    void drawRoiRequested();

    /**
     * @brief ROI检测项配置请求信号
     * @param roiId ROI ID
     */
    void roiDetectionConfigRequested(const QString& roiId);

private slots:
    // ========== 槽函数 ==========

    /**
     * @brief 添加ROI按钮点击
     */
    void onAddRoiClicked();

    /**
     * @brief 删除ROI按钮点击
     */
    void onDeleteRoiClicked();

    /**
     * @brief ROI树形控件项目点击
     * @param item 点击的项目
     * @param column 列号
     */
    void onRoiItemClicked(QTreeWidgetItem* item, int column);

    /**
     * @brief ROI树形控件右键菜单
     * @param pos 右键位置
     */
    void onRoiContextMenuRequested(const QPoint& pos);

    /**
     * @brief ROI树形控件项目双击
     * @param item 双击的项目
     * @param column 列号
     */
    void onRoiItemDoubleClicked(QTreeWidgetItem* item, int column);

    /**
     * @brief 配置检测项按钮点击
     */
    void onConfigureDetectionClicked();

private:
    // ========== UI组件 ==========

    QLabel* m_titleLabel;               // 标题标签
    QPushButton* m_addRoiButton;        // 添加ROI按钮
    QPushButton* m_deleteRoiButton;     // 删除ROI按钮
    QPushButton* m_drawRoiButton;       // 绘制ROI按钮
    QPushButton* m_configDetectionButton; // 配置检测项按钮
    QTreeWidget* m_roiTreeWidget;       // ROI树形控件

    // ========== 数据成员 ==========

    RoiManager* m_roiManager;           // ROI管理器（非拥有）
    QString m_selectedRoiId;            // 当前选中的ROI ID

    // ========== 初始化函数 ==========

    /**
     * @brief 初始化UI
     */
    void initUI();

    /**
     * @brief 初始化信号连接
     */
    void initConnections();

    /**
     * @brief 初始化右键菜单
     */
    void initContextMenu();

    // ========== 辅助函数 ==========

    /**
     * @brief 创建ROI树形项目
     * @param roi ROI配置
     * @return 树形项目
     */
    QTreeWidgetItem* createRoiTreeWidgetItem(const RoiConfig& roi);

    /**
     * @brief 更新树形项目显示
     * @param item 树形项目
     * @param roi ROI配置
     */
    void updateRoiTreeWidgetItem(QTreeWidgetItem* item, const RoiConfig& roi);

    /**
     * @brief 根据ROI ID查找树形项目
     * @param roiId ROI ID
     * @return 树形项目，未找到返回nullptr
     */
    QTreeWidgetItem* findRoiItem(const QString& roiId);

    /**
     * @brief 更新选中状态
     * @param roiId 选中的ROI ID
     */
    void updateSelection(const QString& roiId);

    /**
     * @brief 生成新的ROI名称
     * @return 新ROI名称
     */
    QString generateNewRoiName() const;

    /**
     * @brief 验证ROI名称
     * @param name ROI名称
     * @return 是否有效
     */
    bool validateRoiName(const QString& name) const;
};