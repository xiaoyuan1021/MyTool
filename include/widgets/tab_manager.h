#ifndef TAB_MANAGER_H
#define TAB_MANAGER_H

#include <QObject>
#include <QTabWidget>
#include <QHash>

// 前向声明
class PipelineManager;
class RoiManager;
class ImageView;
class QStatusBar;

/**
 * Tab管理器 - 负责Tab的懒加载、创建和按名获取
 *
 * 职责：
 * 1. 根据名称按需创建Tab Widget（工厂逻辑委托给 TabRegistry）
 * 2. 按名称或类型安全地获取Tab Widget
 * 3. 管理Tab的生命周期
 * 4. 维护Tab名称顺序
 *
 * 信号连接仍由MainWindow在 tabCreated 信号中建立。
 */
class TabManager : public QObject
{
    Q_OBJECT

public:
    explicit TabManager(
        QTabWidget* tabWidget,
        PipelineManager* pipelineManager,
        RoiManager* roiManager,
        ImageView* view,
        QStatusBar* statusBar = nullptr,
        QObject* parent = nullptr
    );
    ~TabManager();

    // ========== 懒加载 ==========

    /// 按需创建Tab（已存在则跳过）
    void ensureTab(const QString& tabName);

    /// 是否已创建
    bool isCreated(const QString& tabName) const;

    /// 获取已创建的Tab名称列表
    QStringList createdTabNames() const { return m_tabOrder; }

    // ========== 类型安全的获取 ==========

    /// 按名称获取 Tab Widget（未创建则返回 nullptr）
    QWidget* getTab(const QString& name) const;

    /// 按名称 + 类型获取 Tab Widget，类型不匹配返回 nullptr
    template<typename T>
    T* getTabAs(const QString& name) const {
        return dynamic_cast<T*>(getTab(name));
    }

    /// 获取所有已注册的 Tab 名称 → Widget 映射（用于 ConfigController 等需要遍历的场景）
    const QHash<QString, QWidget*>& allTabs() const { return m_tabs; }

    /// [NOTE] 清空所有Tab的检测结果（删除图片/ROI时调用）
    void clearAllResults();

signals:
    /// Tab创建后发出，MainWindow可在此建立信号连接
    void tabCreated(const QString& name, QWidget* widget);

private:
    QTabWidget* m_tabWidget;
    PipelineManager* m_pipelineManager;
    RoiManager* m_roiManager;
    ImageView* m_view;
    QStatusBar* m_statusBar;

    // Tab存储：名称 → widget
    QHash<QString, QWidget*> m_tabs;
    QStringList m_tabOrder;
};

#endif // TAB_MANAGER_H