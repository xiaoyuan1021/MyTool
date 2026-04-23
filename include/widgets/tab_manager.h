#ifndef TAB_MANAGER_H
#define TAB_MANAGER_H

#include <QObject>
#include <QTabWidget>
#include <QHash>
#include <memory>

// 前向声明
class PipelineManager;
class RoiManager;
class ImageView;
class QTimer;
class EnhanceTabWidget;
class FilterTabWidget;
class ExtractTabWidget;
class JudgeTabWidget;
class ProcessTabWidget;
class LineDetectTabWidget;
class BarcodeTabWidget;
class ObjectDetectionTabWidget;
class TemplateTabWidget;
class ImageTabWidget;
class VideoTabWidget;

/**
 * Tab管理器 - 负责Tab的懒加载、创建和按名获取
 *
 * 职责：
 * 1. 根据名称按需创建Tab Widget
 * 2. 按名称或类型安全地获取Tab Widget
 * 3. 管理Tab的生命周期（unique_ptr）
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
        QTimer* debounceTimer,
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

    // ========== 按名称获取 ==========

    /// 按名称获取Tab Widget（未创建则返回nullptr）
    QWidget* getTab(const QString& name) const;

    // ========== 类型安全的获取 ==========

    EnhanceTabWidget* getEnhanceTab() const;
    FilterTabWidget* getFilterTab() const;
    ExtractTabWidget* getExtractTab() const;
    JudgeTabWidget* getJudgeTab() const;
    ProcessTabWidget* getProcessTab() const;
    LineDetectTabWidget* getLineDetectTab() const;
    BarcodeTabWidget* getBarcodeTab() const;
    ObjectDetectionTabWidget* getObjectDetectionTab() const;
    TemplateTabWidget* getTemplateTab() const;
    ImageTabWidget* getImageTab() const;
    VideoTabWidget* getVideoTab() const;

signals:
    /// Tab创建后发出，MainWindow可在此建立信号连接
    void tabCreated(const QString& name, QWidget* widget);

private:
    QWidget* createTab(const QString& name);

    QTabWidget* m_tabWidget;
    PipelineManager* m_pipelineManager;
    RoiManager* m_roiManager;
    ImageView* m_view;
    QTimer* m_debounceTimer;

    // Tab存储：名称 → widget（裸指针，析构时释放）
    QHash<QString, QWidget*> m_tabs;
    QStringList m_tabOrder;
};

#endif // TAB_MANAGER_H