#pragma once

#include <functional>
#include <QVector>
#include "config/pipeline_config.h"
#include "core/pipeline.h"

/**
 * @brief Pipeline 访问接口
 *
 * Widget 层通过此接口与 PipelineManager 交互，而非直接依赖具体实现。
 * 降低 Widget → Core 的编译耦合。
 *
 * 使用方式：
 * - Widget 构造函数接受 IPipelineAccess* 参数
 * - Widget 只调用接口方法，不 include pipeline_manager.h
 * - PipelineManager 实现此接口，保持向后兼容
 */
class IPipelineAccess
{
public:
    virtual ~IPipelineAccess() = default;

    // ========== 配置读写 ==========

    /// 获取配置快照（线程安全拷贝）
    virtual PipelineConfig getConfigSnapshot() const = 0;

    /// 设置配置（覆盖当前配置）
    virtual void setConfig(const PipelineConfig& config) = 0;

    /// 批量修改配置（推荐方式）
    virtual void updateConfig(std::function<void(PipelineConfig&)> updater) = 0;

    /// 获取配置的常量引用（仅UI线程调用）
    virtual const PipelineConfig& config() const = 0;

    /// 重置配置为出厂默认值
    virtual void resetConfigToDefaults() = 0;

    // ========== 步骤控制 ==========

    /// 查询指定步骤是否启用
    virtual bool isStepEnabled(int stepIndex) const = 0;

    /// 启用/禁用指定步骤
    virtual void setStepEnabled(int stepIndex, bool enabled) = 0;

    /// 根据当前配置重建 Pipeline
    virtual void rebuildPipeline() = 0;

    // ========== 算法队列 ==========

    /// 添加算法步骤
    virtual void addAlgorithmStep(const AlgorithmStep& step) = 0;

    /// 移除算法步骤
    virtual void removeAlgorithmStep(int index) = 0;

    /// 交换两个算法步骤的位置
    virtual void swapAlgorithmStep(int index1, int index2) = 0;

    /// 获取算法队列
    virtual const QVector<AlgorithmStep>& getAlgorithmQueue() const = 0;

    /// 更新算法步骤
    virtual void updateAlgorithmStep(int index, const AlgorithmStep& step) = 0;

    // ========== 显示与结果 ==========

    /// 设置显示模式
    virtual void setDisplayMode(DisplayConfig::Mode mode) = 0;

    /// 获取最后一次 Pipeline 执行的上下文
    virtual PipelineContext getLastContext() const = 0;
};
