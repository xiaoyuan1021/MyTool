#ifndef DETECTION_UI_CONTROLLER_H
#define DETECTION_UI_CONTROLLER_H

#include <QObject>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QTabWidget>
#include <QStringList>

#include "detection_manager.h"
#include "roi_config.h"

/**
 * 检测项UI控制器 - 负责检测项管理的UI交互逻辑
 *
 * 职责：
 * 1. 检测项管理器初始化
 * 2. 检测项添加/删除操作
 * 3. Tab配置切换
 * 4. TreeView更新
 */
class DetectionUiController : public QObject
{
    Q_OBJECT

public:
    explicit DetectionUiController(
        MultiRoiConfig* multiRoiConfig,
        QTabWidget* tabWidget,
        QObject* parent = nullptr
    );
    ~DetectionUiController();

    // 初始化检测项管理器
    void setupDetectionManager();

    // 获取检测项管理器
    DetectionManager* getDetectionManager() const { return m_detectionManager; }

    // 检测项操作
    void onAddDetectionClicked(const QString& roiId);
    void onDeleteDetectionClicked();
    void onDetectionItemClicked(const QModelIndex& index);

    // Tab配置管理
    void switchToTabConfig(const TabConfig& config);
    void updateTreeView();

    // 设置Tab名称列表（用于动态切换）
    void setTabNames(const QStringList& tabNames) { m_tabNames = tabNames; }

signals:
    // 检测项变更信号
    void detectionChanged();

private:
    MultiRoiConfig* m_multiRoiConfig;
    DetectionManager* m_detectionManager;
    QTabWidget* m_tabWidget;
    QStringList m_tabNames;
};

#endif // DETECTION_UI_CONTROLLER_H