#include "pipeline.h"

// ========== Pipeline类实现 ==========

Pipeline::Pipeline() = default;

void Pipeline::add(std::unique_ptr<IPipelineStep> step)
{
    if (step) {
        steps_.push_back(std::move(step));
    } else {
        qDebug() << "[Pipeline] 警告：尝试添加空步骤";
    }
}

void Pipeline::run(PipelineContext& ctx)
{
    if (steps_.empty()) {
        qDebug() << "[Pipeline] 警告：没有步骤可执行";
        return;
    }

    qDebug() << "[Pipeline] 开始执行，共" << steps_.size() << "个步骤";

    for (size_t i = 0; i < steps_.size(); ++i) {
        auto& step = steps_[i];
        if (step) {
            step->run(ctx);
        } else {
            qDebug() << "[Pipeline] 警告：步骤" << (i + 1) << "为空，跳过";
        }
    }
}
