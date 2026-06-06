#pragma once

#include <functional>
#include "config/pipeline_config.h"

class PipelineManager;
class RoiManager;
class ImageView;
class RoiUiController;
class ProfileManager;
struct PipelineContext;

// ============================================================
// IConfigurableTab — 可配置 Tab Widget 接口
// ============================================================

/**
 * @brief Tab Widget 如需支持配置的保存/加载，应实现此接口。
 * ConfigController 通过此接口与 Tab Widget 交互，无需知道具体类型。
 */
class IConfigurableTab
{
public:
    virtual ~IConfigurableTab() = default;
    virtual void saveToConfig(PipelineConfig& config) const = 0;
    virtual void loadFromConfig(const PipelineConfig& config) = 0;
};

// ============================================================
// ISignalConnectable — Tab Widget 信号连接接口
// ============================================================

/// Tab 信号连接所需的依赖上下文
struct SignalContext {
    PipelineManager* pipelineManager;
    RoiManager* roiManager;
    ImageView* view;
    RoiUiController* roiCtrl;
};

/**
 * @brief 每个需要与外部模块建立信号连接的 Tab Widget 都应实现此接口。
 *
 * 回调说明：
 * - onExecutePipeline: 用户主动触发 Pipeline 执行（切换到 Execute 模式）
 * - onConfigSaved: 配置保存到 PipelineManager（不触发 Pipeline）
 */
class ISignalConnectable
{
public:
    virtual ~ISignalConnectable() = default;
    virtual void connectSignals(const SignalContext& ctx,
                                std::function<void()> onExecutePipeline,
                                std::function<void()> onConfigSaved = nullptr) = 0;
};

// ============================================================
// ITabInitializable — Tab Widget 初始化接口
// ============================================================

/// Tab 初始化上下文，包含各 Tab 可能需要的依赖指针
struct TabInitContext
{
    ProfileManager* profileManager = nullptr;
    PipelineManager* pipelineManager = nullptr;
};

/**
 * @brief Tab Widget 创建后如需执行额外初始化逻辑（如注入依赖），应实现此接口。
 */
class ITabInitializable
{
public:
    virtual ~ITabInitializable() = default;
    virtual void initializeTab(const TabInitContext& ctx) = 0;
};

// ============================================================
// IResultUpdatable — Pipeline 结果更新接口
// ============================================================

/**
 * @brief 需要接收 Pipeline 处理结果的 Tab Widget 实现此接口。
 * PipelineResultHandler 通过此接口分发结果，不再硬编码 Tab 名称。
 */
class IResultUpdatable
{
public:
    virtual ~IResultUpdatable() = default;
    virtual void updateFromPipeline(const PipelineContext& ctx) = 0;
    /// [NOTE] 清空Tab中的检测结果（删除图片/ROI时调用）
    virtual void clearResults() {}
};