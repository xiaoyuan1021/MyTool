#pragma once

#include "pipeline.h"
#include "image_processor.h"
#include "shape_filter_types.h"  // ✅ 引入筛选类型
#include <HalconCpp.h>

using namespace HalconCpp;

// 1) 颜色通道
class StepColorChannel:public IPipelineStep
{
public:
    explicit StepColorChannel(const PipelineConfig* cfg) : cfg_(cfg) {}

    void run(PipelineContext &ctx) override
    {
        if (ctx.srcBgr.empty() || !cfg_) return;
        switch (cfg_->channel)
        {
        case PipelineConfig::Channel::Gray:
            cv::cvtColor(ctx.srcBgr,ctx.channelImg,cv::COLOR_BGR2GRAY);
            break;
        case PipelineConfig::Channel::RGB:
            ctx.channelImg=ctx.srcBgr.clone();
            break;
        case PipelineConfig::Channel::HSV:
            cv::cvtColor(ctx.srcBgr,ctx.channelImg,cv::COLOR_BGR2HSV);
            break;
        case PipelineConfig::Channel::B:
        {
            std::vector<cv::Mat> channels;
            cv::split(ctx.srcBgr,channels);
            ctx.channelImg=channels[0];
            break;
        }
        case PipelineConfig::Channel::G:
        {
            std::vector<cv::Mat> channels;
            cv::split(ctx.srcBgr,channels);
            ctx.channelImg=channels[1];
            break;
        }
        case PipelineConfig::Channel::R:
        {
            std::vector<cv::Mat> channels;
            cv::split(ctx.srcBgr,channels);
            ctx.channelImg=channels[2];
            break;
        }
        default:
            cv::cvtColor(ctx.srcBgr,ctx.channelImg,cv::COLOR_BGR2GRAY);
            break;
        }
    }
private:
    const PipelineConfig* cfg_ = nullptr;
};

// 2) 增强参数
class StepEnhance:public IPipelineStep
{
public:
    StepEnhance(const PipelineConfig* cfg,imageprocessor* proc) : cfg_(cfg), proc_(proc) {}

    void run(PipelineContext& ctx) override
    {
        if(ctx.srcBgr.empty() || !proc_ || !cfg_) return;
        if (ctx.channelImg.empty()) return;

        ctx.enhanced=proc_->adjustParameter(
            ctx.channelImg,
            cfg_->brightness,
            cfg_->contrast,
            cfg_->gamma,
            cfg_->sharpen
            );
    }
private:
    const PipelineConfig* cfg_ = nullptr;
    imageprocessor* proc_=nullptr;
};

// 3) 灰度过滤
class StepGrayFilter : public IPipelineStep
{
public:
    StepGrayFilter(const PipelineConfig* cfg) : cfg_(cfg) {}

    void run(PipelineContext& ctx) override
    {
        if (!cfg_ || !cfg_->enableGrayFilter) {
            ctx.mask.release();
            return;
        }

        if (ctx.enhanced.empty()) {
            return;
        }

        cv::Mat gray;
        if (ctx.enhanced.channels() == 3) {
            cv::cvtColor(ctx.enhanced, gray, cv::COLOR_BGR2GRAY);
        } else {
            gray = ctx.enhanced;
        }

        try {
            if (!gray.isContinuous()) {
                gray = gray.clone();
            }

            HImage hImg("byte", (Hlong)gray.cols, (Hlong)gray.rows, (void*)gray.data);
            HRegion region = hImg.Threshold(
                (double)cfg_->grayLow,
                (double)cfg_->grayHigh
                );

            ctx.mask = ImageUtils::HRegionToMat(region, gray.cols, gray.rows);

            ctx.reason = QString("灰度过滤: 范围[%1,%2]")
                             .arg(cfg_->grayLow)
                             .arg(cfg_->grayHigh);
        }
        catch (const HalconCpp::HException& ex) {
            qDebug() << "Halcon Threshold error:" << ex.ErrorMessage().Text();
            ctx.mask.release();
        }
    }

private:
    const PipelineConfig* cfg_ = nullptr;
};

// 4) 算法队列
class StepAlgorithmQueue : public IPipelineStep
{
public:
    StepAlgorithmQueue(imageprocessor* proc, const QVector<AlgorithmStep>* queue)
        : m_processor(proc), m_algorithmQueue(queue) {}

    void run(PipelineContext& ctx) override
    {
        if (!m_processor || !m_algorithmQueue || m_algorithmQueue->isEmpty())
        {
            return;
        }

        cv::Mat input;
        if (!ctx.mask.empty())
        {
            input = ctx.mask;
        }
        else if (!ctx.enhanced.empty())
        {
            if (ctx.enhanced.channels() == 3) {
                cv::cvtColor(ctx.enhanced, input, cv::COLOR_BGR2GRAY);
            } else {
                input = ctx.enhanced;
            }
        }
        else
        {
            if (ctx.srcBgr.channels() == 3) {
                cv::cvtColor(ctx.srcBgr, input, cv::COLOR_BGR2GRAY);
            } else {
                input = ctx.srcBgr;
            }
        }

        if (input.empty()) return;

        cv::Mat result = m_processor->executeAlgorithmQueue(input, *m_algorithmQueue);

        if (!result.empty())
        {
            ctx.processed = result;
            ctx.reason = QString("算法队列执行完成 (%1个步骤)")
                             .arg(m_algorithmQueue->size());
        }
    }

private:
    imageprocessor* m_processor = nullptr;
    const QVector<AlgorithmStep>* m_algorithmQueue = nullptr;
};

// ✅ 5) 形状筛选（新）- 支持多特征、多条件
class StepShapeFilter : public IPipelineStep
{
public:
    explicit StepShapeFilter(const PipelineConfig* cfg) : m_cfg(cfg) {}

    void run(PipelineContext& ctx) override
    {
        if (!m_cfg || ctx.processed.empty()) {
            return;
        }

        const ShapeFilterConfig& filter = m_cfg->shapeFilter;

        if (!filter.hasValidConditions()) {
            return;
        }

        try {
            HRegion inputRegion = ImageUtils::MatToHRegion(ctx.processed);
            HRegion connectedRegions = inputRegion.Connection();

            HTuple numBefore;
            HalconCpp::CountObj(connectedRegions, &numBefore);

            qDebug() << "========== 形状筛选 ==========";
            qDebug() << "筛选模式:" << getFilterModeName(filter.mode);
            qDebug() << "筛选前区域数量:" << numBefore[0].I();

            HRegion filteredRegion = applyFilter(connectedRegions, filter);

            HRegion connectedResult = filteredRegion.Connection();
            HTuple numAfter;
            HalconCpp::CountObj(connectedResult, &numAfter);

            qDebug() << "筛选后区域数量:" << numAfter[0].I();
            qDebug() << "==============================";

            ctx.currentRegions=numAfter[0].I();

            cv::Mat resultMat = ImageUtils::HRegionToMat(
                filteredRegion,
                ctx.processed.cols,
                ctx.processed.rows
                );

            if (!resultMat.empty()) {
                ctx.processed = resultMat;
                ctx.reason = QString("形状筛选: %1, 保留 %2/%3 个区域")
                                 .arg(filter.toString())
                                 .arg((int)numAfter[0].I())
                                 .arg((int)numBefore[0].I());
            }
        }
        catch (const HalconCpp::HException& ex)
        {
            qDebug() << "[ShapeFilter] Halcon错误:" << ex.ErrorMessage().Text();
            ctx.reason = "形状筛选失败";
        }
    }

private:
    HRegion applyFilter(const HRegion& regions, const ShapeFilterConfig& config)
    {
        if (config.mode == FilterMode::And) {
            return applyFilterAnd(regions, config);
        } else {
            return applyFilterOr(regions, config);
        }
    }

    HRegion applyFilterAnd(const HRegion& regions, const ShapeFilterConfig& config)
    {
        HRegion result = regions;

        for (const auto& cond : config.conditions)
        {
            if (!cond.isValid()) continue;

            qDebug() << QString("  应用条件: %1").arg(cond.toString());

            result = result.SelectShape(
                getFeatureName(cond.feature),
                "and",
                cond.minValue,
                cond.maxValue
                );

            HTuple count;
            HalconCpp::CountObj(result, &count);
            qDebug() << QString("    剩余区域: %1").arg((int)count[0].I());
        }

        return result;
    }

    HRegion applyFilterOr(const HRegion& regions, const ShapeFilterConfig& config)
    {
        HRegion result;
        bool hasResult = false;

        for (const auto& cond : config.conditions)
        {
            if (!cond.isValid()) continue;

            qDebug() << QString("  应用条件: %1").arg(cond.toString());

            HRegion singleResult = regions.SelectShape(
                getFeatureName(cond.feature),
                "and",
                cond.minValue,
                cond.maxValue
                );

            HTuple count;
            HalconCpp::CountObj(singleResult, &count);
            qDebug() << QString("    该条件匹配区域: %1").arg((int)count[0].I());

            if (!hasResult) {
                result = singleResult;
                hasResult = true;
            } else {
                result = result.Union2(singleResult);
            }
        }

        return hasResult ? result : HRegion();
    }

private:
    const PipelineConfig* m_cfg = nullptr;
};

// pipeline_steps.h

// ✅ 新增：颜色过滤步骤
class StepColorFilter : public IPipelineStep
{
public:
    StepColorFilter(const PipelineConfig* cfg, imageprocessor* proc)
        : m_cfg(cfg), m_processor(proc) {}

    void run(PipelineContext& ctx) override
    {
        if (!m_cfg->enableColorFilter) {
            return;  // 未启用，直接返回
        }

        cv::Mat input = ctx.enhanced.empty() ? ctx.srcBgr : ctx.enhanced;

        cv::Mat filterMask;
        switch (m_cfg->colorFilterMode)
        {
        case PipelineConfig::ColorFilterMode::RGB:
            filterMask = m_processor->filterRGB(
                input,
                m_cfg->rLow, m_cfg->rHigh,
                m_cfg->gLow, m_cfg->gHigh,
                m_cfg->bLow, m_cfg->bHigh
                );
            break;

        case PipelineConfig::ColorFilterMode::HSV:
            filterMask = m_processor->filterHSV(
                input,
                m_cfg->hLow, m_cfg->hHigh,
                m_cfg->sLow, m_cfg->sHigh,
                m_cfg->vLow, m_cfg->vHigh
                );
            break;

        default:
            return;
        }

        // 与现有 mask 合并（如果有的话）
        if (!ctx.mask.empty()) {
            cv::bitwise_and(ctx.mask, filterMask, ctx.mask);
        } else {
            ctx.mask = filterMask;
        }

        ctx.reason = QString("颜色过滤: %1 模式")
                         .arg(m_cfg->colorFilterMode == PipelineConfig::ColorFilterMode::RGB ? "RGB" : "HSV");
    }

private:
    const PipelineConfig* m_cfg;
    imageprocessor* m_processor;
};
