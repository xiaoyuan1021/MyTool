#include "pipeline_steps.h"
#include <opencv2/line_descriptor.hpp>

void StepColorChannel::run(PipelineContext &ctx)
{
    if (ctx.srcBgr.empty() || !cfg_) return;
    switch (cfg_->channel)
    {
    case PipelineConfig::Channel::Gray:
        cv::cvtColor(ctx.srcBgr, ctx.channelImg, cv::COLOR_BGR2GRAY);
        break;
    case PipelineConfig::Channel::RGB:
        ctx.channelImg = ctx.srcBgr;
        break;
    case PipelineConfig::Channel::HSV:
        cv::cvtColor(ctx.srcBgr, ctx.channelImg, cv::COLOR_BGR2HSV);
        break;
    case PipelineConfig::Channel::B:
        cv::extractChannel(ctx.srcBgr, ctx.channelImg, 0);
        break;
    case PipelineConfig::Channel::G:
        cv::extractChannel(ctx.srcBgr, ctx.channelImg, 1);
        break;
    case PipelineConfig::Channel::R:
        cv::extractChannel(ctx.srcBgr, ctx.channelImg, 2);
        break;
    default:
        ctx.channelImg = ctx.srcBgr;
    }
}

void StepEnhance::run(PipelineContext& ctx)
{
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

}

void StepGrayFilter::run(PipelineContext& ctx)
{
    if (cfg_->currentFilterMode != PipelineConfig::FilterMode::Gray)
    {
        return;
    }

    if (!cfg_ || !cfg_->enableGrayFilter)
    {
        ctx.mask.release();
        return;
    }

    if (ctx.enhanced.empty())
    {
        return;
    }

    cv::Mat gray;
    if (ctx.enhanced.channels() == 3)
    {
        cv::cvtColor(ctx.enhanced, gray, cv::COLOR_BGR2GRAY);
    } else {
        gray = ctx.enhanced;
    }

    try
    {
        if (!gray.isContinuous())
        {
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
    catch (const HalconCpp::HException& ex)
    {
        qDebug() << "Halcon Threshold error:" << ex.ErrorMessage().Text();
        ctx.mask.release();
    }
}

void StepAlgorithmQueue::run(PipelineContext& ctx)
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


void StepShapeFilter::run(PipelineContext& ctx)
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

HRegion StepShapeFilter::applyFilter(const HRegion& regions, const ShapeFilterConfig& config)
    {
        if (config.mode == FilterMode::And) {
            return applyFilterAnd(regions, config);
        } else {
            return applyFilterOr(regions, config);
        }
    }

HRegion StepShapeFilter::applyFilterAnd(const HRegion& regions, const ShapeFilterConfig& config)
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

HRegion StepShapeFilter::applyFilterOr(const HRegion& regions, const ShapeFilterConfig& config)
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

void StepColorFilter::run(PipelineContext& ctx) 
    {
        if (m_cfg->currentFilterMode != PipelineConfig::FilterMode::RGB &&
            m_cfg->currentFilterMode != PipelineConfig::FilterMode::HSV) {
            return;  // 不是颜色过滤模式，直接返回
        }

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

        cv::bitwise_not(filterMask,ctx.mask);//影响性能吗

        ctx.reason = QString("颜色过滤: %1 模式")
                         .arg(m_cfg->colorFilterMode == PipelineConfig::ColorFilterMode::RGB ? "RGB" : "HSV");
    }

// ========== 直线检测辅助函数 ==========

static void detectLinesHoughP(const cv::Mat& edges, const PipelineConfig* cfg, std::vector<cv::Vec4f>& lines)
{
    std::vector<cv::Vec4i> linesInt;
    cv::HoughLinesP(edges, linesInt, cfg->lineRho, cfg->lineTheta,
                   cfg->lineThreshold, cfg->lineMinLength, cfg->lineMaxGap);

    // 转换为 Vec4f
    for (const auto& line : linesInt) {
        lines.push_back(cv::Vec4f(line[0], line[1], line[2], line[3]));
    }

    qDebug() << "[HoughP] 检测到" << lines.size() << "条直线";
}

static void detectLinesLSD(const cv::Mat& src, std::vector<cv::Vec4f>& lines)
{
    auto LSDDetector = cv::line_descriptor::LSDDetector::createLSDDetector();
    std::vector<cv::line_descriptor::KeyLine> keylines;
    LSDDetector->detect(src, keylines, 2, 1);
    qDebug() << "[LSD] 检测到" << keylines.size() << "条直线";
    for (const auto& kl : keylines)
    {
        cv::Point2f p1 = kl.getStartPoint();
        cv::Point2f p2 = kl.getEndPoint();
        if (p1 != p2) {
            lines.push_back(cv::Vec4f(p1.x, p1.y, p2.x, p2.y));
        }
    }
}

static void detectLinesEDlines(const cv::Mat& src, std::vector<cv::Vec4f>& lines)
{
    auto edDetector = cv::line_descriptor::BinaryDescriptor::createBinaryDescriptor();
    std::vector<cv::line_descriptor::KeyLine> keylines;
    edDetector->detect(src, keylines);
    qDebug() << "[EDlines] 检测到" << keylines.size() << "条直线";

    for (const auto& kl : keylines)
    {
        cv::Point2f p1 = kl.getStartPoint();
        cv::Point2f p2 = kl.getEndPoint();
        if (p1 != p2) {
            lines.push_back(cv::Vec4f(p1.x, p1.y, p2.x, p2.y));
        }
    }
}

static void EDdrawing(const cv::Mat& src, cv::Ptr<cv::ximgproc::EdgeDrawing> ed, PipelineContext& ctx)
{
    ed = cv::ximgproc::createEdgeDrawing();
    ed->detectEdges(src);
    std::vector<std::vector<cv::Point>> segments = ed->getSegments();
    for (const auto& seg : segments) 
        {
            for (size_t i = 0; i < seg.size() - 1; i++) 
            {
            cv::line(ctx.lineDetect, seg[i], seg[i+1], cv::Scalar(0, 255, 0), 1);
            }
        }
}

void StepLineDetect::run(PipelineContext &ctx) 
    {
        if (!cfg_ || !cfg_->enableLineDetect) return;
        if (ctx.srcBgr.empty()) return;

        cv::Mat src = ctx.srcBgr;
        cv::Mat gray;
        cv::cvtColor(src, gray, cv::COLOR_BGR2GRAY);

        // 添加边缘检测
        cv::Mat edges;
        cv::Canny(gray, edges, 50, 150);

        std::vector<cv::Vec4f> lines;

        // 根据算法选择
        if (cfg_->lineDetectAlgorithm == 0)
        {
            detectLinesHoughP(edges, cfg_, lines);
        }
        else if (cfg_->lineDetectAlgorithm == 1)
        {
            detectLinesLSD(gray, lines);
        }
        else if (cfg_->lineDetectAlgorithm == 2)
        {
            detectLinesEDlines(src, lines);
        }

        // 绘制直线到lineDetect
        ctx.lineDetect = src.clone();

        // HoughP和LSD用端点连线
        if (cfg_->lineDetectAlgorithm == 0 || cfg_->lineDetectAlgorithm == 1)
        {
            for (const auto& line : lines)
            {
                cv::line(ctx.lineDetect,
                        cv::Point(line[0], line[1]),
                        cv::Point(line[2], line[3]),
                        cv::Scalar(0, 255, 0), 1);
            }
        }
        // EDlines用EdgeDrawing绘制完整边缘链
        else if (cfg_->lineDetectAlgorithm == 2)
        {
            cv::Ptr<cv::ximgproc::EdgeDrawing> ed;
            EDdrawing(gray, ed, ctx);
        }
    }