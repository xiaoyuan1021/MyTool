#ifndef DETECTION_UI_CONTROLLER_H
#define DETECTION_UI_CONTROLLER_H

#include <QObject>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QTabWidget>
#include <QStringList>
#include <functional>

#include "roi_config.h"
#include "roi_manager.h"

// 前向声明
class RoiUiController;
class TabManager;
class PipelineManager;
class JudgeTabWidget;
struct BlobAnalysisConfig;

/**
 * 检测项UI控制器 - 负责检测项管理的UI交互逻辑
 *
 * 职责：
 * 1. 检测项添加/删除操作
 * 2. 检测项选中跳转
 * 3. Tab配置切换
 * 4. TreeView更新
 */
class DetectionUiController : public QObject
{
    Q_OBJECT

public:
    explicit DetectionUiController(
        RoiManager& roiManager,
        QTabWidget* tabWidget,
        QObject* parent = nullptr
    );
    ~DetectionUiController();

    // 检测项操作
    void onAddDetectionClicked(const QString& roiId);
    void onDeleteDetectionClicked(const QString& roiId, const QString& detectionId);

    /// 从QTreeView中获取当前选中的检测项并执行删除（封装Tree节点解析逻辑）
    /// @param treeWidget 树控件
    /// @return true表示成功执行了删除操作，false表示没有选中有效检测项
    bool handleDeleteFromTree(QTreeWidget* treeWidget);

    // 检测项选中跳转
    void onDetectionItemSelected(const QString& roiId, const QString& detectionId,
                                 TabManager* tabManager, PipelineManager* pipelineManager);

    // Tab配置管理
    void switchToTabConfig(const TabConfig& config);

    // 设置Tab名称列表（用于动态切换）
    void setTabNames(const QStringList& tabNames) { m_tabNames = tabNames; }

    /// 将UI信号连接到Controller（从MainWindow迁移）
    /// @param triggerPipeline Pipeline执行触发回调
    void setupConnections(RoiUiController* roiController,
                          std::function<void(const QString&)> ensureTabFunc,
                          std::function<void()> triggerPipeline = nullptr);

signals:
    // 检测项变更信号
    void detectionChanged();
    
    // 请求创建Tab信号（懒加载）
    void ensureTabNeeded(const QString& tabName);

    // 请求更新Tab可见性（检测项删除后调用）
    void tabVisibilityUpdateNeeded();

private:
    RoiManager& m_roiManager;
    QTabWidget* m_tabWidget;
    QStringList m_tabNames;
    std::function<void()> m_triggerPipeline;  // Pipeline执行触发回调
};

#endif // DETECTION_UI_CONTROLLER_H
