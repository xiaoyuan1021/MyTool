#include "pipeline.h"
#include "logger.h"

// ========== Pipeline类实现 ==========

Pipeline::Pipeline() = default;

// ========== 步骤管理 ==========

void Pipeline::add(std::unique_ptr<IPipelineStep> step)
{
    if (step) {
        steps_.push_back(std::move(step));
    } else {
        Logger::instance()->info("[Pipeline] 警告：尝试添加空步骤");
    }
}

bool Pipeline::remove(int index)
{
    if (index < 0 || index >= static_cast<int>(steps_.size())) {
        qDebug() << "[Pipeline] 警告：尝试移除无效索引" << index;
        return false;
    }
    
    steps_.erase(steps_.begin() + index);
    qDebug() << "[Pipeline] 已移除步骤" << index;
    return true;
}

void Pipeline::clear()
{
    steps_.clear();
    Logger::instance()->info("[Pipeline] 已清空所有步骤");
}

size_t Pipeline::size() const
{
    return steps_.size();
}

bool Pipeline::empty() const
{
    return steps_.empty();
}

bool Pipeline::swap(int index1, int index2)
{
    if (index1 < 0 || index1 >= static_cast<int>(steps_.size()) ||
        index2 < 0 || index2 >= static_cast<int>(steps_.size())) {
        qDebug() << "[Pipeline] 警告：尝试交换无效索引" << index1 << "和" << index2;
        return false;
    }
    
    std::swap(steps_[index1], steps_[index2]);
    qDebug() << "[Pipeline] 已交换步骤" << index1 << "和" << index2;
    return true;
}

const IPipelineStep* Pipeline::getStep(int index) const
{
    if (index < 0 || index >= static_cast<int>(steps_.size())) {
        return nullptr;
    }
    return steps_[index].get();
}

IPipelineStep* Pipeline::getStep(int index)
{
    if (index < 0 || index >= static_cast<int>(steps_.size())) {
        return nullptr;
    }
    return steps_[index].get();
}

// ========== 执行 ==========

void Pipeline::run(PipelineContext& ctx)
{
    if (steps_.empty()) {
        Logger::instance()->info("[Pipeline] 警告：没有步骤可执行");
        return;
    }

    for (size_t i = 0; i < steps_.size(); ++i) {
        auto& step = steps_[i];
        if (step) {
            if (ctx.config && !ctx.config->stepEnabled[static_cast<int>(step->stepType())]) {
                continue;
            }
            step->run(ctx);
        }
    }
}
