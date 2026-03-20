#pragma once

#include "pipeline.h"
#include "image_processor.h"
#include "shape_filter_types.h"
#include <HalconCpp.h>
#include <opencv2/ximgproc.hpp>

using namespace HalconCpp;

// 1) 颜色通道
class StepColorChannel:public IPipelineStep
{
public:
    explicit StepColorChannel(const PipelineConfig* cfg) : cfg_(cfg) {}

    void run(PipelineContext &ctx) override;
private:
    const PipelineConfig* cfg_ = nullptr;
};

// 2) 增强参数
class StepEnhance:public IPipelineStep
{
public:
    StepEnhance(const PipelineConfig* cfg,imageprocessor* proc) : cfg_(cfg), proc_(proc) {}

    void run(PipelineContext& ctx) override;
private:
    const PipelineConfig* cfg_ = nullptr;
    imageprocessor* proc_=nullptr;
};

// 3) 灰度过滤
class StepGrayFilter : public IPipelineStep
{
public:
    StepGrayFilter(const PipelineConfig* cfg) : cfg_(cfg) {}

    void run(PipelineContext& ctx) override;

private:
    const PipelineConfig* cfg_ = nullptr;
};

// 4) 算法队列
class StepAlgorithmQueue : public IPipelineStep
{
public:
    StepAlgorithmQueue(imageprocessor* proc, const QVector<AlgorithmStep>* queue)
        : m_processor(proc), m_algorithmQueue(queue) {}

    void run(PipelineContext& ctx) override;
private:
    imageprocessor* m_processor = nullptr;
    const QVector<AlgorithmStep>* m_algorithmQueue = nullptr;
};

// 5) 形状筛选
class StepShapeFilter : public IPipelineStep
{
public:
    explicit StepShapeFilter(const PipelineConfig* cfg) : m_cfg(cfg) {}

    void run(PipelineContext& ctx) override;
private:
    HRegion applyFilter(const HRegion& regions, const ShapeFilterConfig& config);
    
    HRegion applyFilterAnd(const HRegion& regions, const ShapeFilterConfig& config);
    
    HRegion applyFilterOr(const HRegion& regions, const ShapeFilterConfig& config);

private:
    const PipelineConfig* m_cfg = nullptr;
};

// 颜色过滤步骤
class StepColorFilter : public IPipelineStep
{
public:
    StepColorFilter(const PipelineConfig* cfg, imageprocessor* proc)
        : m_cfg(cfg), m_processor(proc) {}

    void run(PipelineContext& ctx) override;
private:
    const PipelineConfig* m_cfg;
    imageprocessor* m_processor;
};

// 直线检测
class StepLineDetect : public IPipelineStep
{
public:
    explicit StepLineDetect(const PipelineConfig* cfg) : cfg_(cfg) {}

    void run(PipelineContext &ctx) override;
private:
    const PipelineConfig* cfg_;
};

// 参考线匹配
class StepReferenceLineFilter : public IPipelineStep
{
public:
    explicit StepReferenceLineFilter(const PipelineConfig* cfg) : cfg_(cfg) {}

    void run(PipelineContext &ctx) override;
private:
    const PipelineConfig* cfg_ = nullptr;
};

