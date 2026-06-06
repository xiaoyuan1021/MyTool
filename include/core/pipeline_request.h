#pragma once

#include <opencv2/core.hpp>
#include "config/pipeline_config.h"
#include <QAtomicInt>
#include <QString>
#include <chrono>

/**
 * Pipeline请求 - 不可变值对象
 * 
 * 设计原则：
 * 1. 一旦创建就不能修改，确保线程安全
 * 2. 包含执行所需的所有信息
 * 3. 唯一标识符用于去重和取消
 */
class PipelineRequest
{
public:
    PipelineRequest(const cv::Mat& image, const PipelineConfig& config, int priority = 0, const QString& caller = {})
        : m_image(image.clone())
        , m_config(config)
        , m_priority(priority)
        , m_timestamp(std::chrono::steady_clock::now().time_since_epoch().count())
        , m_id(s_nextId.fetchAndAddOrdered(1))
        , m_caller(caller)
    {
    }

    // 移动构造和赋值
    PipelineRequest(PipelineRequest&&) = default;
    PipelineRequest& operator=(PipelineRequest&&) = default;

    // 拷贝构造和赋值（深拷贝图像）
    PipelineRequest(const PipelineRequest& other)
        : m_image(other.m_image.clone())
        , m_config(other.m_config)
        , m_priority(other.m_priority)
        , m_timestamp(other.m_timestamp)
        , m_id(other.m_id)
        , m_caller(other.m_caller)
    {
    }
    
    PipelineRequest& operator=(const PipelineRequest& other)
    {
        if (this != &other) {
            m_image = other.m_image.clone();
            m_config = other.m_config;
            m_priority = other.m_priority;
            m_timestamp = other.m_timestamp;
            m_id = other.m_id;
            m_caller = other.m_caller;
        }
        return *this;
    }

    // 只读访问
    const cv::Mat& image() const { return m_image; }
    const PipelineConfig& config() const { return m_config; }
    int priority() const { return m_priority; }
    qint64 timestamp() const { return m_timestamp; }
    qint64 id() const { return m_id; }
    const QString& caller() const { return m_caller; }

    // 创建快照（深拷贝图像，保留 ID 和 caller）
    PipelineRequest snapshot() const
    {
        PipelineRequest req(m_image, m_config, m_priority, m_caller);
        req.m_id = m_id;  // 保留原始 ID
        return req;
    }

private:
    // 带 ID 的构造（仅用于 snapshot）
    PipelineRequest(const cv::Mat& image, const PipelineConfig& config, int priority, const QString& caller, qint64 id)
        : m_image(image.clone())
        , m_config(config)
        , m_priority(priority)
        , m_timestamp(std::chrono::steady_clock::now().time_since_epoch().count())
        , m_id(id)
        , m_caller(caller)
    {
    }

    cv::Mat m_image;
    PipelineConfig m_config;
    int m_priority;
    qint64 m_timestamp;
    qint64 m_id;
    QString m_caller;

    // 全局ID生成器
    static QAtomicInt s_nextId;
};
