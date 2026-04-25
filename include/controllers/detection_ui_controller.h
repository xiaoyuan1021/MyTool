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

    // 检测项选中跳转
    void onDetectionItemSelected(const QString& roiId, const QString& detectionId,
                                 TabManager* tabManager, PipelineManager* pipelineManager);

    // Tab配置管理
    void switchToTabConfig(const TabConfig& config);
    void switchToTabConfigByType(DetectionType type);

    // 设置Tab名称列表（用于动态切换）
    void setTabNames(const QStringList& tabNames) { m_tabNames = tabNames; }

    /// 将UI信号连接到Controller（从MainWindow迁移）
    void setupConnections(RoiUiController* roiController, std::function<void(const QString&)> ensureTabFunc);

signals:
    // 检测项变更信号
    void detectionChanged();
    
    // 请求创建Tab信号（懒加载）
    void ensureTabNeeded(const QString& tabName);

private:
    RoiManager& m_roiManager;
    QTabWidget* m_tabWidget;
    QStringList m_tabNames;
};

#endif // DETECTION_UI_CONTROLLER_H
