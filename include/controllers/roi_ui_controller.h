#ifndef ROI_UI_CONTROLLER_H
#define ROI_UI_CONTROLLER_H

#include <QObject>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QPoint>
#include <QRectF>
#include <QString>
#include <QStatusBar>
#include <QMessageBox>

#include "roi_config.h"
#include "roi_manager.h"
#include "image_view.h"
#include "pipeline_manager.h"

// 前向声明
class RoiListWidget;
class TabManager;

QT_BEGIN_NAMESPACE
class QStatusBar;
QT_END_NAMESPACE

/**
 * ROI UI控制器 - 负责ROI相关的UI交互逻辑
 *
 * 职责：
 * 1. ROI按钮事件处理
 * 2. ROI树形视图管理
 * 3. ROI选择和切换逻辑
 */
class RoiUiController : public QObject
{
    Q_OBJECT

public:
    explicit RoiUiController(
        RoiManager& roiManager,
        PipelineManager* pipelineManager,
        ImageView* imageView,
        QStatusBar* statusBar,
        QObject* parent = nullptr
    );
    ~RoiUiController();

    // 设置树形控件
    void setupTreeView(QTreeWidget* treeView);

    // ROI按钮事件处理
    void onDrawRoiClicked();
    void onResetRoiClicked();
    void onAddRoiClicked();
    void onDeleteRoiClicked();
    void onSwitchRoiClicked();
    
    // 统一的ROI选择逻辑（点击ROI/检测项时调用）
    void handleRoiSelection(const QString& roiId);

    // ROI操作方法（供MainWindow委托调用）
    QString addRoiWithName(const QString& roiName);
    bool renameRoi(const QString& roiId, const QString& newName);
    void deleteDetectionItem(const QString& roiId, const QString& detectionId);

    // ROI树形视图管理
    void refreshRoiTreeView();
    void onRoiTreeItemClicked(QTreeWidgetItem* item, int column);

    // 获取当前选中的ROI ID
    QString getCurrentSelectedRoiId() const { return m_currentSelectedRoiId; }

    // 保存当前ROI的PipelineConfig（在切换ROI前调用）
    void saveCurrentRoiPipelineConfig();
    
    // 加载指定ROI的PipelineConfig到PipelineManager
    void loadRoiPipelineConfig(const QString& roiId);
    
    // 设置RoiListWidget并建立信号连接
    void setupRoiListWidget(RoiListWidget* roiListWidget);

    /// 更新当前选中ROI的 Blob 检测项判定阈值（由 JudgeTab 触发）
    void updateBlobDetectionConfig(int minCount, int maxCount);

    /// 设置显示相关的信号连接（从MainWindow迁移）
    void setupDisplayConnections(TabManager* tabManager);

    // 同步当前图片的ROI配置到RoiListWidget（图片切换时调用）
    void syncRoiConfigsToWidget();

    // 获取RoiManager引用
    RoiManager& getRoiManager() { return m_roiManager; }

signals:
    // ROI配置修改信号（需要重新执行Pipeline处理）
    void roiChanged();
    
    // ROI显示切换信号（纯显示切换，不需要Pipeline处理）
    // 用于：切换原图/ROI显示、点击ROI列表项切换显示
    void roiDisplayChanged(const QString& roiId);
    
    // 切换到原图显示信号（纯显示切换，不需要Pipeline处理）
    void fullImageDisplayRequested();
    
    // 检测项选中信号（用于触发Tab切换）
    void detectionItemSelected(const QString& roiId, const QString& detectionId);
    
    // ROI切换信号（通知MainWindow刷新EnhanceTabWidget的UI）
    void roiPipelineConfigChanged(const PipelineConfig& config);
    
    // 检测项删除信号
    void detectionItemDeleted(const QString& roiId, const QString& detectionId);
    
    // ROI重命名信号
    void roiRenamed(const QString& roiId, const QString& newName);
    
    // ROI激活状态变化信号
    void roiActiveToggled(const QString& roiId, bool active);

private slots:
    // RoiListWidget信号处理槽函数
    void handleRoiAddRequested(const QString& roiName);
    void handleRoiDeleteRequested(const QString& roiId);
    void handleRoiRenameRequested(const QString& roiId, const QString& newName);
    void handleRoiActiveChanged(const QString& roiId, bool active);
    void handleRoiSelectionChanged(const QString& roiId);

private:
    /// 进入ROI绘制模式（统一入口）
    void enterRoiDrawMode();

    /// 删除ROI并刷新UI（统一删除逻辑，供 onDeleteRoiClicked 和 handleRoiDeleteRequested 调用）
    void removeRoiAndRefresh(const QString& roiId);

    RoiManager& m_roiManager;
    PipelineManager* m_pipelineManager;
    ImageView* m_view;
    QStatusBar* m_statusBar;
    QTreeWidget* m_treeView;
    RoiListWidget* m_roiListWidget;  // RoiListWidget引用
    QString m_currentSelectedRoiId;
};

#endif // ROI_UI_CONTROLLER_H