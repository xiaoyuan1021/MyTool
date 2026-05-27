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

// 3) 统一过滤（灰度/RGB/HSV）
class StepFilter : public IPipelineStep
{
public:
    StepFilter(ImageProcessor* proc) : m_processor(proc) {}
    void run(PipelineContext& ctx) override;
private:
    ImageProcessor* m_processor = nullptr;
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

// 6) 直线检测（含参考线匹配）
class StepLineDetector : public IPipelineStep
{
public:
    void run(PipelineContext& ctx) override;
private:
    void runLineDetect(PipelineContext& ctx);
    void runReferenceLineMatch(PipelineContext& ctx);
};

// 滤波去噪
class StepImageFilter : public IPipelineStep
{
public:
    void run(PipelineContext& ctx) override;
};
