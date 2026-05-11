#pragma once

#include <QString>

struct AppConfig;

/**
 * @brief 可配置 Tab Widget 接口
 *
 * Tab Widget 如需支持配置的保存/加载，应实现此接口。
 * ConfigController 通过此接口与 Tab Widget 交互，无需知道具体类型。
 *
 * 新增 Tab 时只需：① 继承此接口 ② 实现 saveToConfig / loadFromConfig
 * ConfigController 永远不需要修改。
 */
class IConfigurableTab
{
public:
    virtual ~IConfigurableTab() = default;

    /**
     * @brief 将当前 UI 配置保存到 AppConfig
     * @param config 目标配置对象
     */
    virtual void saveToConfig(AppConfig& config) const = 0;

    /**
     * @brief 从 AppConfig 加载配置到 UI
     * @param config 源配置对象
     */
    virtual void loadFromConfig(const AppConfig& config) = 0;
};