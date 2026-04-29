#ifndef DISPLAY_MODE_MANAGER_H
#define DISPLAY_MODE_MANAGER_H

#include <QObject>
#include <QHash>
#include <QString>

#include "config/display_config.h"

class QTabWidget;
class PipelineManager;
class ImageView;

/**
 * 显示模式管理器 - 负责Tab页面与Pipeline显示模式之间的映射与切换
 *
 * 职责：
 * 1. 维护 Tab名称 → DisplayMode 映射表
 * 2. 根据当前Tab切换Pipeline的显示模式
 * 3. 管理快速渲染（利用上次Pipeline结果，不重新执行）
 *
 * 从 MainWindow 中提取，降低 MainWindow 的职责复杂度。
 */
class DisplayModeManager : public QObject
{
    Q_OBJECT

public:
    explicit DisplayModeManager(QTabWidget* tabWidget,
                                PipelineManager* pipelineManager,
                                ImageView* view,
                                QObject* parent = nullptr);

    /// 获取指定Tab的显示模式
    DisplayConfig::Mode getModeForTab(int index) const;

signals:
    /// 当没有上次Pipeline结果，需要触发完整处理时发出
    void requestRefresh();

public slots:
    /// Tab切换时调用：判断显示模式是否变化，决定是快速渲染还是触发Pipeline
    void onTabChanged(int index);

    /// 在Pipeline执行前调用：根据当前Tab设置Pipeline的显示模式
    void applyModeForCurrentTab();

private:
    /// 用上次Pipeline结果快速渲染当前显示模式
    /// @return true表示快速渲染成功，false表示需要触发完整Pipeline
    bool displayCurrentResult();

    QTabWidget* m_tabWidget;
    PipelineManager* m_pipelineManager;
    ImageView* m_view;

    /// 当前显示模式，用于判断Tab切换是否需要刷新
    DisplayConfig::Mode m_lastMode = DisplayConfig::Mode::Original;
};

#endif // DISPLAY_MODE_MANAGER_H