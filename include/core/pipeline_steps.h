#pragma once

#include "pipeline.h"
#include "image_processor.h"
#include "shape_filter_types.h"

// 1) 颜色通道
class StepColorChannel:public IPipelineStep
{
public:
    void run(PipelineContext &ctx) override;
};

// 2) 增强参数
class StepEnhance:public IPipelineStep
{
public:
    explicit StepEnhance(ImageProcessor* proc) : proc_(proc) {}

    void run(PipelineContext& ctx) override;
private:
    ImageProcessor* proc_ = nullptr;
};

// 3) 灰度过滤
class StepGrayFilter : public IPipelineStep
{
public:
    void run(PipelineContext& ctx) override;
};

// 4) 算法队列
class StepAlgorithmQueue : public IPipelineStep
{
public:
    StepAlgorithmQueue(ImageProcessor* proc, const QVector<AlgorithmStep>* queue)
        : m_processor(proc), m_algorithmQueue(queue) {}

    void run(PipelineContext& ctx) override;
private:
    ImageProcessor* m_processor = nullptr;
    const QVector<AlgorithmStep>* m_algorithmQueue = nullptr;
};

// 5) 形状筛选
class StepShapeFilter : public IPipelineStep
{
public:
    void run(PipelineContext& ctx) override;
private:
    cv::Mat applyFilter(const cv::Mat& regions, const ShapeFilterConfig& config);

    cv::Mat applyFilterAnd(const cv::Mat& regions, const ShapeFilterConfig& config);

    cv::Mat applyFilterOr(const cv::Mat& regions, const ShapeFilterConfig& config);
};

// 颜色过滤步骤
class StepColorFilter : public IPipelineStep
{
public:
    StepColorFilter(ImageProcessor* proc) : m_processor(proc) {}

    void run(PipelineContext& ctx) override;
private:
    ImageProcessor* m_processor;
};

// 直线检测
class StepLineDetect : public IPipelineStep
{
public:
    void run(PipelineContext &ctx) override;
};

// 参考线匹配
class StepReferenceLineFilter : public IPipelineStep
{
public:
    void run(PipelineContext &ctx) override;
};

// 滤波去噪
class StepImageFilter : public IPipelineStep
{
public:
    void run(PipelineContext& ctx) override;
};
