#pragma once

#include <QObject>

struct PipelineContext;

/**
 * @brief Pipeline 结果更新接口
 *
 * 需要接收 Pipeline 处理结果的 Tab Widget 实现此接口。
 * PipelineResultHandler 通过此接口分发结果，不再硬编码 Tab 名称。
 *
 * 新增检测类型时只需：
 *   ① 在 Tab Widget 中继承此接口
 *   ② 实现 updateFromPipeline()
 *   PipelineResultHandler 永远不需要修改。
 */
class IResultUpdatable
{
public:
    virtual ~IResultUpdatable() = default;

    /**
     * @brief 从 Pipeline 结果更新 UI
     * @param ctx Pipeline 执行结果上下文
     *
     * 实现者应只处理自己关心的结果字段，忽略不相关的字段。
     */
    virtual void updateFromPipeline(const PipelineContext& ctx) = 0;
};