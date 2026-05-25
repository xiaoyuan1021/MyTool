#pragma once

#include "pipeline_request.h"
#include "pipeline.h"
#include <QString>

/**
 * Pipeline结果 - 不可变值对象
 * 
 * 包含执行结果和元数据，用于线程间传递
 */
class PipelineResult
{
public:
    // 创建成功结果
    static PipelineResult success(PipelineRequest request, PipelineContext context, double elapsedMs)
    {
        return PipelineResult(std::move(request), std::move(context), true, QString(), elapsedMs);
    }

    // 创建失败结果
    static PipelineResult failure(PipelineRequest request, QString error, double elapsedMs = 0)
    {
        PipelineContext emptyCtx;
        return PipelineResult(std::move(request), std::move(emptyCtx), false, std::move(error), elapsedMs);
    }

    // 只读访问
    const PipelineRequest& request() const { return m_request; }
    const PipelineContext& context() const { return m_context; }
    double elapsedMs() const { return m_elapsedMs; }
    bool isSuccess() const { return m_success; }
    QString errorMessage() const { return m_errorMessage; }
    qint64 requestId() const { return m_request.id(); }

private:
    PipelineResult(PipelineRequest request, PipelineContext context, 
                   bool success, QString error, double elapsedMs)
        : m_request(std::move(request))
        , m_context(std::move(context))
        , m_success(success)
        , m_errorMessage(std::move(error))
        , m_elapsedMs(elapsedMs)
    {
    }

    PipelineRequest m_request;
    PipelineContext m_context;
    bool m_success;
    QString m_errorMessage;
    double m_elapsedMs;
};
