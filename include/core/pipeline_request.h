#pragma once

#include <opencv2/core.hpp>
#include "config/pipeline_config.h"
#include <QAtomicInt>
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
    PipelineRequest(const cv::Mat& image, const PipelineConfig& config, int priority = 0)
        : m_image(image.clone())
        , m_config(config)
        , m_priority(priority)
        , m_timestamp(std::chrono::steady_clock::now().time_since_epoch().count())
        , m_id(s_nextId.fetchAndAddOrdered(1))
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
        }
        return *this;
    }

    // 只读访问
    const cv::Mat& image() const { return m_image; }
    const PipelineConfig& config() const { return m_config; }
    int priority() const { return m_priority; }
    qint64 timestamp() const { return m_timestamp; }
    qint64 id() const { return m_id; }

    // 创建快照（深拷贝图像）
    PipelineRequest snapshot() const
    {
        PipelineRequest req(m_image, m_config, m_priority);
        return req;
    }

private:
    cv::Mat m_image;
    PipelineConfig m_config;
    int m_priority;
    qint64 m_timestamp;
    qint64 m_id;

    // 全局ID生成器
    static QAtomicInt s_nextId;
};
