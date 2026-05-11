#ifndef CONFIG_CONTROLLER_H
#define CONFIG_CONTROLLER_H

#include <QObject>
#include <QString>
#include <QList>

#include "config_manager.h"
#include "pipeline_manager.h"
#include "roi_manager.h"

class IConfigurableTab;

/**
 * 配置控制器 - 负责配置的保存和加载
 *
 * 职责：
 * 1. 保存配置到文件
 * 2. 从文件加载配置
 * 3. 通过 IConfigurableTab 接口收集/应用 UI 配置
 * 4. 管理 ROI 和 Pipeline 的配置
 *
 * 设计改进：
 * - 不再持有具体 Tab Widget 指针，改用 IConfigurableTab 接口列表
 * - 新增 Tab 时只需实现 IConfigurableTab 接口并注册，ConfigController 无需修改
 */
class ConfigController : public QObject
{
    Q_OBJECT

public:
    explicit ConfigController(
        PipelineManager* pipelineManager,
        RoiManager& roiManager,
        QObject* parent = nullptr
    );

    /**
     * @brief 注册一个可配置的 Tab Widget
     * @param tab 实现了 IConfigurableTab 接口的 Tab Widget
     *
     * 推荐在 MainWindow 构造完成后调用，或在 tabCreated 信号中调用。
     * ConfigController 不拥有 tab 的生命周期。
     */
    void registerTab(IConfigurableTab* tab);

    /**
     * @brief 批量注册可配置的 Tab Widget
     * @param tabs Tab Widget 列表
     */
    void registerTabs(const QList<IConfigurableTab*>& tabs);

    // 保存配置
    void saveConfig(QWidget* parentWidget);

    // 加载配置
    void loadConfig(QWidget* parentWidget);

signals:
    // 配置已应用信号
    void configApplied();

private:
    // 通过 IConfigurableTab 接口从所有已注册 Tab 收集配置
    void collectConfigFromUI(AppConfig& config);

    // 通过 IConfigurableTab 接口将配置应用到所有已注册 Tab
    void applyConfigToUI(const AppConfig& config);

    PipelineManager* m_pipelineManager;
    RoiManager& m_roiManager;

    // 通过接口解耦的 Tab Widget 列表
    QList<IConfigurableTab*> m_configurableTabs;
};

#endif // CONFIG_CONTROLLER_H