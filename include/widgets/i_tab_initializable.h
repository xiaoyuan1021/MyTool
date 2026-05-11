#pragma once

/**
 * @brief Tab Widget 初始化接口
 *
 * Tab Widget 创建后如需执行额外初始化逻辑（如注入依赖），
 * 应实现此接口。TabManager 创建 Tab 后自动调用 initializeTab()。
 *
 * 用法：
 * - 实现 initializeTab() 方法，在其中完成依赖注入等初始化
 * - MainWindow 不再需要按名称硬编码判断
 *
 * 示例：
 * ```cpp
 * class TemplateTabWidget : public QWidget, public ITabInitializable {
 *     void initializeTab(InitContext& ctx) override {
 *         setProfileManager(ctx.profileManager);
 *     }
 * };
 * ```
 */

class ProfileManager;

/// Tab 初始化上下文，包含各 Tab 可能需要的依赖指针
struct TabInitContext
{
    ProfileManager* profileManager = nullptr;
    class PipelineManager* pipelineManager = nullptr;
};

class ITabInitializable
{
public:
    virtual ~ITabInitializable() = default;

    /**
     * @brief Tab 创建后由 TabManager/MainWindow 调用
     * @param ctx 初始化上下文，包含可选的依赖指针
     */
    virtual void initializeTab(const TabInitContext& ctx) = 0;
};