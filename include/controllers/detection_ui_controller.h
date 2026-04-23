#ifndef DETECTION_UI_CONTROLLER_H
#define DETECTION_UI_CONTROLLER_H

#include <QObject>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QTabWidget>
#include <QStringList>

#include "roi_config.h"

/**
 * 检测项UI控制器 - 负责检测项管理的UI交互逻辑
 *
 * 职责：
 * 1. 检测项添加/删除操作
 * 2. Tab配置切换
 * 3. TreeView更新
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

    // 检测项操作
    void onAddDetectionClicked(const QString& roiId);
    void onDeleteDetectionClicked();

    // Tab配置管理
    void switchToTabConfig(const TabConfig& config);
    void switchToTabConfigByType(DetectionType type);

    // 设置Tab名称列表（用于动态切换）
    void setTabNames(const QStringList& tabNames) { m_tabNames = tabNames; }

signals:
    // 检测项变更信号
    void detectionChanged();
    
    // 请求创建Tab信号（懒加载）
    void ensureTabNeeded(const QString& tabName);

private:
    MultiRoiConfig* m_multiRoiConfig;
    QTabWidget* m_tabWidget;
    QStringList m_tabNames;
};

#endif // DETECTION_UI_CONTROLLER_H
