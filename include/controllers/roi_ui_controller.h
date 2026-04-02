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
        MultiRoiConfig* multiRoiConfig,
        RoiManager& roiManager,
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
    void onRoiSelected(const QRectF& roiRect);

    // ROI树形视图管理
    void refreshRoiTreeView();
    void onRoiTreeItemClicked(QTreeWidgetItem* item, int column);
    void onRoiTreeItemDoubleClicked(QTreeWidgetItem* item, int column);
    void onRoiTreeContextMenuRequested(const QPoint& pos);

    // 获取当前选中的ROI ID
    QString getCurrentSelectedRoiId() const { return m_currentSelectedRoiId; }

signals:
    // ROI变更信号（用于通知主窗口更新显示）
    void roiChanged();
    
    // 检测项选中信号（用于触发Tab切换）
    void detectionItemSelected(const QString& roiId, const QString& detectionId);

private:
    MultiRoiConfig* m_multiRoiConfig;
    RoiManager& m_roiManager;
    ImageView* m_view;
    QStatusBar* m_statusBar;
    QTreeWidget* m_treeView;
    QString m_currentSelectedRoiId;
};

#endif // ROI_UI_CONTROLLER_H