#ifndef CONFIG_CONTROLLER_H
#define CONFIG_CONTROLLER_H

#include <QObject>
#include <QString>

#include "config_manager.h"
#include "pipeline_manager.h"
#include "roi_manager.h"

class EnhanceTabWidget;
class FilterTabWidget;
class ExtractTabWidget;
class JudgeTabWidget;
class ProcessTabWidget;

/**
 * 配置控制器 - 负责配置的保存和加载
 *
 * 职责：
 * 1. 保存配置到文件
 * 2. 从文件加载配置
 * 3. 收集UI配置
 * 4. 应用配置到UI
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
    ~ConfigController();

    // 设置需要访问的Tab Widget
    void setTabWidgets(
        EnhanceTabWidget* enhanceTab,
        FilterTabWidget* filterTab,
        ExtractTabWidget* extractTab,
        JudgeTabWidget* judgeTab,
        ProcessTabWidget* processTab
    );

    // 保存配置
    void saveConfig(QWidget* parentWidget);

    // 加载配置
    void loadConfig(QWidget* parentWidget);

signals:
    // 配置已应用信号
    void configApplied();

private:
    // 从UI收集配置
    void collectConfigFromUI(AppConfig& config);

    // 应用配置到UI
    void applyConfigToUI(const AppConfig& config);

    PipelineManager* m_pipelineManager;
    RoiManager& m_roiManager;

    // Tab Widget指针
    EnhanceTabWidget* m_enhanceTabWidget;
    FilterTabWidget* m_filterTabWidget;
    ExtractTabWidget* m_extractTabWidget;
    JudgeTabWidget* m_judgeTabWidget;
    ProcessTabWidget* m_processTabWidget;
};

#endif // CONFIG_CONTROLLER_H