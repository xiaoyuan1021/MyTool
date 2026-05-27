#pragma once

#include <QString>
#include <QHash>
#include <functional>
#include <QVector>
#include "config/display_config.h"

class QWidget;
class PipelineManager;
class ImageView;
class RoiManager;
class QStatusBar;

/**
 * @brief Tab 元数据条目
 *
 * 每个 Tab 的完整信息，定义在一处。
 * TabManager、DisplayModeManager、PipelineResultHandler 都从此注册表查询，
 * 新增 Tab 时只需在此注册一次。
 */
struct TabEntry {
    QString name;                              // Tab 显示名称（如 "增强"）
    DisplayConfig::Mode displayMode;           // 该 Tab 对应的显示模式
    bool needsInitialize = false;              // 创建后是否需要调用 initialize()
};

/**
 * @brief Tab 元数据注册表（单例）
 *
 * 集中管理所有 Tab 的名称→显示模式映射、工厂函数等元数据。
 * 消除 TabManager、DisplayModeManager、PipelineResultHandler 中的硬编码。
 */
class TabRegistry
{
public:
    using FactoryFunc = std::function<QWidget*(PipelineManager*, ImageView*, RoiManager*, QStatusBar*)>;

    static TabRegistry& instance();

    /// 注册一个 Tab
    void registerTab(const TabEntry& entry, FactoryFunc factory);

    /// 获取所有已注册的 Tab 条目（按注册顺序）
    const QVector<TabEntry>& entries() const { return m_entries; }

    /// 按名称查找 Tab 条目，未找到返回 nullptr
    const TabEntry* find(const QString& name) const;

    /// 按名称查找对应的显示模式，未找到返回 Original
    DisplayConfig::Mode displayModeFor(const QString& name) const;

    /// 创建指定名称的 Tab Widget，未注册返回 nullptr
    QWidget* createTab(const QString& name,
                       PipelineManager* pm, ImageView* view,
                       RoiManager* rm, QStatusBar* statusBar = nullptr) const;

private:
    TabRegistry();

    QVector<TabEntry> m_entries;
    QHash<QString, int> m_nameIndex;   // name -> index in m_entries
    QHash<QString, FactoryFunc> m_factories;
};