#include "pipeline_steps.h"
#include "opencv_algorithm.h"
#include "logger.h"
#include <opencv2/line_descriptor.hpp>
#include <cmath>
#include <algorithm>

void StepColorChannel::run(PipelineContext &ctx)
{
    if (ctx.srcBgr.empty() || !ctx.config) return;
    try {
        switch (ctx.config->colorFilter.channel)
        {
        case ChannelMode::Gray:
            cv::cvtColor(ctx.srcBgr, ctx.channelImg, cv::COLOR_BGR2GRAY);
            break;
        case ChannelMode::RGB:
            ctx.channelImg = ctx.srcBgr;
            break;
        case ChannelMode::HSV:
            cv::cvtColor(ctx.srcBgr, ctx.channelImg, cv::COLOR_BGR2HSV);
            break;
        case ChannelMode::B:
            cv::extractChannel(ctx.srcBgr, ctx.channelImg, 0);
            break;
        case ChannelMode::G:
            cv::extractChannel(ctx.srcBgr, ctx.channelImg, 1);
            break;
        case ChannelMode::R:
            cv::extractChannel(ctx.srcBgr, ctx.channelImg, 2);
            break;
        default:
            ctx.channelImg = ctx.srcBgr;
        }
        ctx.visualBase = ctx.channelImg;
    } catch (const cv::Exception& ex) {
        qDebug() << "[ColorChannel] OpenCV错误:" << ex.what();
    Logger::instance()->error(QString("ColorChannel OpenCV错误: %1").arg(ex.what()));
        ctx.channelImg = ctx.srcBgr;
        ctx.visualBase = ctx.channelImg;
    } catch (...) {
        qDebug() << "[ColorChannel] 未知异常";
    Logger::instance()->error(QString("ColorChannel 未知异常"));
        ctx.channelImg = ctx.srcBgr;
        ctx.visualBase = ctx.channelImg;
    }
}

void StepEnhance::run(PipelineContext& ctx)
{
    {
        if(ctx.srcBgr.empty() || !proc_ || !ctx.config) return;
        if (ctx.channelImg.empty()) return;

        try {
            // EnhanceConfig 使用与 UI 滑块一致的 int 原始值
            // 这里转换为算法所需的比例值（brightness 不需要转换）
            ctx.enhanced=proc_->adjustParameter(
                ctx.channelImg,
                ctx.config->enhance.brightness,
                ctx.config->enhance.contrast / 100.0,
                ctx.config->enhance.gamma / 100.0,
                ctx.config->enhance.sharpen / 100.0
                );
            ctx.visualBase = ctx.enhanced;
        } catch (const cv::Exception& ex) {
            qDebug() << "[Enhance] OpenCV错误:" << ex.what();
    Logger::instance()->error(QString("Enhance OpenCV错误: %1").arg(ex.what()));
            ctx.enhanced = ctx.channelImg;
            ctx.visualBase = ctx.enhanced;
        } catch (...) {
            qDebug() << "[Enhance] 未知异常";
    Logger::instance()->error(QString("Enhance 未知异常"));
            ctx.enhanced = ctx.channelImg;
            ctx.visualBase = ctx.enhanced;
        }
    }

}

void StepGrayFilter::run(PipelineContext& ctx)
{
    if (!ctx.config) return;

    // 如果不是灰度过滤模式，释放mask并返回
    if (ctx.config->colorFilter.currentFilterMode != ImageFilterMode::Gray)
    {
        ctx.mask.release();
        return;
    }

    if (!ctx.config->colorFilter.enableGrayFilter)
    {
        ctx.mask.release();
        return;
    }

    if (ctx.enhanced.empty())
    {
        ctx.mask.release();
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

        // 灰度范围 [grayLow, grayHigh] 内的像素设为255（绿色/目标），其余为0（白色/背景）
        cv::inRange(gray, cv::Scalar(ctx.config->colorFilter.grayLow), cv::Scalar(ctx.config->colorFilter.grayHigh), ctx.mask);

        ctx.reason = QString("灰度过滤: 范围[%1,%2]")
                         .arg(ctx.config->colorFilter.grayLow)
                         .arg(ctx.config->colorFilter.grayHigh);
    }
    catch (const cv::Exception& ex)
    {
        qDebug() << "OpenCV灰度过滤错误:" << ex.what();
        Logger::instance()->error(QString("灰度过滤OpenCV错误: %1").arg(ex.what()));
        ctx.mask.release();
    }
}

void StepAlgorithmQueue::run(PipelineContext& ctx)
    {
        if (!m_processor || !m_algorithmQueue || m_algorithmQueue->isEmpty())
        {
            return;
        }

        try {
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
        } catch (const cv::Exception& ex) {
            qDebug() << "[AlgorithmQueue] OpenCV错误:" << ex.what();
    Logger::instance()->error(QString("AlgorithmQueue OpenCV错误: %1").arg(ex.what()));
            ctx.reason = "算法队列执行失败";
        } catch (...) {
            qDebug() << "[AlgorithmQueue] 未知异常";
    Logger::instance()->error(QString("AlgorithmQueue 未知异常"));
            ctx.reason = "算法队列执行失败";
        }
    }


void StepShapeFilter::run(PipelineContext& ctx)
    {
        if (!ctx.config) return;
        cv::Mat inputMat = !ctx.processed.empty() ? ctx.processed : ctx.mask;

        if (inputMat.empty()) {
            return;
        }

        const ShapeFilterConfig& filter = ctx.config->shapeFilter;

        if (!filter.hasValidConditions()) {
            return;
        }

        try {
            cv::Mat binary;
            if (inputMat.channels() == 3) {
                cv::cvtColor(inputMat, binary, cv::COLOR_BGR2GRAY);
            } else {
                binary = inputMat.clone();
            }
            cv::threshold(binary, binary, 127, 255, cv::THRESH_BINARY);

            cv::Mat labels, stats, centroids;
            int numBefore = cv::connectedComponentsWithStats(binary, labels, stats, centroids, 8) - 1;

            qDebug() << "========== 形状筛选 ==========";
            qDebug() << "筛选模式:" << getFilterModeName(filter.mode);
            qDebug() << "筛选前区域数量:" << numBefore;

            cv::Mat filteredRegion = applyFilter(binary, filter);

            cv::Mat filteredLabels, filteredStats, filteredCentroids;
            int numAfter = cv::connectedComponentsWithStats(filteredRegion, filteredLabels, filteredStats, filteredCentroids, 8) - 1;

            qDebug() << "筛选后区域数量:" << numAfter;
            qDebug() << "==============================";

            ctx.currentRegions = numAfter;

            if (!filteredRegion.empty()) {
                ctx.processed = filteredRegion;
                ctx.reason = QString("形状筛选: %1, 保留 %2/%3 个区域")
                                 .arg(filter.toString())
                                 .arg(numAfter)
                                 .arg(numBefore);
            }
        }
        catch (const cv::Exception& ex)
        {
            qDebug() << "[ShapeFilter] OpenCV错误:" << ex.what();
    Logger::instance()->error(QString("ShapeFilter OpenCV错误: %1").arg(ex.what()));
            ctx.reason = "形状筛选失败";
        }
    }

cv::Mat StepShapeFilter::applyFilter(const cv::Mat& regions, const ShapeFilterConfig& config)
    {
        if (config.mode == ShapeFilterLogicMode::And) {
            return applyFilterAnd(regions, config);
        } else {
            return applyFilterOr(regions, config);
        }
    }

cv::Mat StepShapeFilter::applyFilterAnd(const cv::Mat& regions, const ShapeFilterConfig& config)
    {
        cv::Mat result = regions.clone();

        for (const auto& cond : config.conditions)
        {
            if (!cond.isValid()) continue;

            qDebug() << QString("  应用条件: %1").arg(cond.toString());

            result = OpenCVAlgorithm::selectShapeByFeature(
                result,
                QString(getFeatureName(cond.feature)),
                cond.minValue,
                cond.maxValue
            );

            cv::Mat labels, stats, centroids;
            int count = cv::connectedComponentsWithStats(result, labels, stats, centroids, 8) - 1;
            qDebug() << QString("    剩余区域: %1").arg(count);
        }

        return result;
    }

cv::Mat StepShapeFilter::applyFilterOr(const cv::Mat& regions, const ShapeFilterConfig& config)
    {
        cv::Mat result = cv::Mat::zeros(regions.size(), CV_8UC1);
        bool hasResult = false;

        for (const auto& cond : config.conditions)
        {
            if (!cond.isValid()) continue;

            qDebug() << QString("  应用条件: %1").arg(cond.toString());

            cv::Mat singleResult = OpenCVAlgorithm::selectShapeByFeature(
                regions,
                QString(getFeatureName(cond.feature)),
                cond.minValue,
                cond.maxValue
            );

            cv::Mat labels, stats, centroids;
            int count = cv::connectedComponentsWithStats(singleResult, labels, stats, centroids, 8) - 1;
            qDebug() << QString("    该条件匹配区域: %1").arg(count);

            if (!hasResult) {
                result = singleResult;
                hasResult = true;
            } else {
                cv::bitwise_or(result, singleResult, result);
            }
        }

        return hasResult ? result : cv::Mat();
    }

void StepColorFilter::run(PipelineContext& ctx)
    {
        if (!ctx.config) return;

        // 如果是灰度过滤模式，不处理（让StepGrayFilter处理mask）
        if (ctx.config->colorFilter.currentFilterMode == ImageFilterMode::Gray) {
            return;
        }

        // 如果不是颜色过滤模式（RGB/HSV），释放mask并返回
        if (ctx.config->colorFilter.currentFilterMode != ImageFilterMode::RGB &&
            ctx.config->colorFilter.currentFilterMode != ImageFilterMode::HSV) {
            ctx.mask.release();
            return;
        }

        if (!ctx.config->colorFilter.enableColorFilter) {
            ctx.mask.release();
            return;
        }

        try {
            cv::Mat input = ctx.enhanced.empty() ? ctx.srcBgr : ctx.enhanced;

            cv::Mat filterMask;
            switch (ctx.config->colorFilter.colorFilterMode)
            {
            case ColorFilterMode::RGB:
                filterMask = m_processor->filterRGB(
                    input,
                    ctx.config->colorFilter.rLow, ctx.config->colorFilter.rHigh,
                    ctx.config->colorFilter.gLow, ctx.config->colorFilter.gHigh,
                    ctx.config->colorFilter.bLow, ctx.config->colorFilter.bHigh
                    );
                break;

            case ColorFilterMode::HSV:
                filterMask = m_processor->filterHSV(
                    input,
                    ctx.config->colorFilter.hLow, ctx.config->colorFilter.hHigh,
                    ctx.config->colorFilter.sLow, ctx.config->colorFilter.sHigh,
                    ctx.config->colorFilter.vLow, ctx.config->colorFilter.vHigh
                    );
                break;

            default:
                return;
            }

            cv::bitwise_not(filterMask,ctx.mask);

            ctx.reason = QString("颜色过滤: %1 模式")
                             .arg(ctx.config->colorFilter.colorFilterMode == ColorFilterMode::RGB ? "RGB" : "HSV");
        } catch (const cv::Exception& ex) {
            qDebug() << "[ColorFilter] OpenCV错误:" << ex.what();
    Logger::instance()->error(QString("ColorFilter OpenCV错误: %1").arg(ex.what()));
            ctx.mask.release();
            ctx.reason = "颜色过滤失败";
        } catch (...) {
            qDebug() << "[ColorFilter] 未知异常";
    Logger::instance()->error(QString("ColorFilter 未知异常"));
            ctx.mask.release();
            ctx.reason = "颜色过滤失败";
        }
    }

// ========== 直线检测辅助函数 ==========

static void detectLinesHoughP(const cv::Mat& edges, const PipelineConfig* cfg, std::vector<cv::Vec4f>& lines)
{
    if (!cfg || edges.empty()) return;
    try {
        std::vector<cv::Vec4i> linesInt;
        cv::HoughLinesP(edges, linesInt, cfg->lineDetect.rho, cfg->lineDetect.theta,
                       cfg->lineDetect.threshold, cfg->lineDetect.minLength, cfg->lineDetect.maxGap);

        for (const auto& line : linesInt) {
            lines.push_back(cv::Vec4f(line[0], line[1], line[2], line[3]));
        }

        qDebug() << "[HoughP] 检测到" << lines.size() << "条直线";
    } catch (const cv::Exception& ex) {
        qDebug() << "[HoughP] OpenCV错误:" << ex.what();
    Logger::instance()->error(QString("HoughP OpenCV错误: %1").arg(ex.what()));
    } catch (...) {
        qDebug() << "[HoughP] 未知异常";
    Logger::instance()->error(QString("HoughP 未知异常"));
    }
}

static void detectLinesLSD(const cv::Mat& src, std::vector<cv::Vec4f>& lines)
{
    if (src.empty()) return;
    try {
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
    } catch (const cv::Exception& ex) {
        qDebug() << "[LSD] OpenCV错误:" << ex.what();
    Logger::instance()->error(QString("LSD OpenCV错误: %1").arg(ex.what()));
    } catch (...) {
        qDebug() << "[LSD] 未知异常";
    Logger::instance()->error(QString("LSD 未知异常"));
    }
}

static void detectLinesEDlines(const cv::Mat& src, std::vector<cv::Vec4f>& lines)
{
    if (src.empty()) return;
    try {
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
    } catch (const cv::Exception& ex) {
        qDebug() << "[EDlines] OpenCV错误:" << ex.what();
    Logger::instance()->error(QString("EDlines OpenCV错误: %1").arg(ex.what()));
    } catch (...) {
        qDebug() << "[EDlines] 未知异常";
    Logger::instance()->error(QString("EDlines 未知异常"));
    }
}

void StepLineDetect::run(PipelineContext &ctx)
    {
        if (!ctx.config || !ctx.config->lineDetect.enabled) return;
        if (ctx.srcBgr.empty()) return;

        try {
            cv::Mat srcVis = ctx.visualBase.empty() ? ctx.srcBgr : ctx.visualBase;
            cv::Mat srcGray;
            cv::Mat srcColor;

            if (srcVis.channels() == 1) {
                srcGray = srcVis;
                cv::cvtColor(srcVis, srcColor, cv::COLOR_GRAY2BGR);
            } else {
                cv::cvtColor(srcVis, srcGray, cv::COLOR_BGR2GRAY);
                srcColor = srcVis;
            }

            cv::Mat edges;
            cv::Canny(srcGray, edges, 50, 150);

            std::vector<cv::Vec4f> lines;

            if (ctx.config->lineDetect.algorithm == 0)
            {
                detectLinesHoughP(edges, ctx.config, lines);
            }
            else if (ctx.config->lineDetect.algorithm == 1)
            {
                detectLinesLSD(srcGray, lines);
            }
            else if (ctx.config->lineDetect.algorithm == 2)
            {
                detectLinesEDlines(srcGray, lines);
            }

            ctx.lineDetect = srcColor.clone();

            for (const auto& line : lines)
            {
                cv::line(ctx.lineDetect,
                        cv::Point(line[0], line[1]),
                        cv::Point(line[2], line[3]),
                        cv::Scalar(0, 255, 0), 1);
            }

            ctx.totalLineCount = static_cast<int>(lines.size());
        } catch (const cv::Exception& ex) {
            qDebug() << "[LineDetect] OpenCV错误:" << ex.what();
    Logger::instance()->error(QString("LineDetect OpenCV错误: %1").arg(ex.what()));
            ctx.reason = "直线检测失败";
        } catch (...) {
            qDebug() << "[LineDetect] 未知异常";
    Logger::instance()->error(QString("LineDetect 未知异常"));
            ctx.reason = "直线检测失败";
        }
    }

// ========== 参考线匹配辅助函数 ==========

static double calculateLineAngle(const cv::Vec4f& line)
{
    cv::Point2f p1(line[0], line[1]);
    cv::Point2f p2(line[2], line[3]);
    double angle = std::atan2(p2.y - p1.y, p2.x - p1.x) * 180.0 / CV_PI;
    return angle;
}

static double pointToLineDistance(const cv::Point2f& point, 
                                  const cv::Point2f& lineStart, 
                                  const cv::Point2f& lineEnd)
{
    cv::Point2f lineVec = lineEnd - lineStart;
    cv::Point2f pointVec = point - lineStart;
    
    double lineLength = cv::norm(lineVec);
    if (lineLength < 1e-6) {
        return cv::norm(pointVec);
    }
    
    double cross = std::abs(lineVec.x * pointVec.y - lineVec.y * pointVec.x);
    return cross / lineLength;
}

static double calculateAngleDifference(double angle1, double angle2)
{
    double diff = std::abs(angle1 - angle2);
    if (diff > 180.0) {
        diff = 360.0 - diff;
    }
    return diff;
}

static bool isLineMatchingReference(const cv::Vec4f& line,
                                    const cv::Point2f& refStart,
                                    const cv::Point2f& refEnd,
                                    double refAngle,
                                    double angleThreshold,
                                    double distanceThreshold)
{
    double lineAngle = calculateLineAngle(line);
    double angleDiff = calculateAngleDifference(lineAngle, refAngle);
    
    if (angleDiff > angleThreshold) {
        return false;
    }
    
    cv::Point2f lineMid((line[0] + line[2]) / 2.0f, (line[1] + line[3]) / 2.0f);
    double dist = pointToLineDistance(lineMid, refStart, refEnd);
    
    if (dist > distanceThreshold) {
        return false;
    }
    
    return true;
}

static double calculateReferenceLineAngle(const PipelineConfig* cfg)
{
    return std::atan2(cfg->lineDetect.referenceLineEnd.y - cfg->lineDetect.referenceLineStart.y,
                      cfg->lineDetect.referenceLineEnd.x - cfg->lineDetect.referenceLineStart.x) * 180.0 / CV_PI;
}

// ========== 参考线匹配步骤 ==========

void StepReferenceLineFilter::run(PipelineContext& ctx)
{
    if (!ctx.config || !ctx.config->lineDetect.enableReferenceLineMatch) {
        return;
    }

    if (!ctx.config->lineDetect.referenceLineValid) {
        ctx.reason = "参考线未绘制";
        return;
    }

    if (ctx.srcBgr.empty()) {
        return;
    }

    try {
        cv::Mat srcVis = ctx.visualBase.empty() ? ctx.srcBgr : ctx.visualBase;
        cv::Mat src;
        if (srcVis.channels() == 1)
            cv::cvtColor(srcVis, src, cv::COLOR_GRAY2BGR);
        else
            src = srcVis;

        cv::Point2f refStart = ctx.config->lineDetect.referenceLineStart;
        cv::Point2f refEnd = ctx.config->lineDetect.referenceLineEnd;

        cv::Point2f lineDir = refEnd - refStart;
        double lineLength = cv::norm(lineDir);
        if (lineLength < 1e-6) {
            ctx.reason = "参考线长度太短";
            return;
        }

        cv::Point2f normal(-lineDir.y / lineLength, lineDir.x / lineLength);

        double halfWidth = ctx.config->lineDetect.searchRegionWidth / 2.0;
        cv::Point2f offset = normal * halfWidth;

        std::vector<cv::Point> searchRegion = {
            cv::Point(refStart.x + offset.x, refStart.y + offset.y),
            cv::Point(refEnd.x + offset.x, refEnd.y + offset.y),
            cv::Point(refEnd.x - offset.x, refEnd.y - offset.y),
            cv::Point(refStart.x - offset.x, refStart.y - offset.y)
        };

        cv::Mat regionMask = cv::Mat::zeros(src.size(), CV_8UC1);
        std::vector<std::vector<cv::Point>> contours = {searchRegion};
        cv::fillPoly(regionMask, contours, cv::Scalar(255));

        cv::Mat gray;
        cv::cvtColor(src, gray, cv::COLOR_BGR2GRAY);

        cv::Mat maskedGray;
        gray.copyTo(maskedGray, regionMask);

        cv::Mat edges;
        cv::Canny(maskedGray, edges, 50, 150);

        std::vector<cv::Vec4f> allLines;

        if (ctx.config->lineDetect.algorithm == 0) {
            detectLinesHoughP(edges, ctx.config, allLines);
        } else if (ctx.config->lineDetect.algorithm == 1) {
            detectLinesLSD(maskedGray, allLines);
        } else if (ctx.config->lineDetect.algorithm == 2) {
            detectLinesEDlines(maskedGray, allLines);
        }

        double refAngle = calculateReferenceLineAngle(ctx.config);

        std::vector<cv::Vec4f> matchedLines;
        for (const auto& line : allLines) {
            if (isLineMatchingReference(line,
                                        ctx.config->lineDetect.referenceLineStart,
                                        ctx.config->lineDetect.referenceLineEnd,
                                        refAngle,
                                        ctx.config->lineDetect.angleThreshold,
                                        ctx.config->lineDetect.distanceThreshold)) {
                matchedLines.push_back(line);
            }
        }

        ctx.lineDetect = src.clone();

        cv::line(ctx.lineDetect,
                 ctx.config->lineDetect.referenceLineStart,
                 ctx.config->lineDetect.referenceLineEnd,
                 cv::Scalar(0, 255, 255), 4, cv::LINE_AA);

        for (const auto& line : matchedLines) {
            cv::line(ctx.lineDetect,
                     cv::Point(line[0], line[1]),
                     cv::Point(line[2], line[3]),
                     cv::Scalar(255, 0, 255), 3, cv::LINE_AA);
        }

        for (const auto& line : allLines) {
            bool isMatched = false;
            for (const auto& matched : matchedLines) {
                if (std::abs(line[0] - matched[0]) < 1 &&
                    std::abs(line[1] - matched[1]) < 1 &&
                    std::abs(line[2] - matched[2]) < 1 &&
                    std::abs(line[3] - matched[3]) < 1) {
                    isMatched = true;
                    break;
                }
            }
            if (!isMatched) {
                cv::line(ctx.lineDetect,
                         cv::Point(line[0], line[1]),
                         cv::Point(line[2], line[3]),
                         cv::Scalar(0, 255, 0), 1, cv::LINE_AA);
            }
        }

        ctx.reason = QString("参考线匹配: 找到 %1/%2 条匹配直线 (角度容差:%3°, 距离容差:%4px)")
                         .arg(matchedLines.size())
                         .arg(allLines.size())
                         .arg(ctx.config->lineDetect.angleThreshold)
                         .arg(ctx.config->lineDetect.distanceThreshold);

        ctx.matchedLineCount = static_cast<int>(matchedLines.size());
        ctx.totalLineCount = static_cast<int>(allLines.size());

        qDebug() << "[ReferenceLineFilter]" << ctx.reason;
    } catch (const cv::Exception& ex) {
        qDebug() << "[ReferenceLineFilter] OpenCV错误:" << ex.what();
    Logger::instance()->error(QString("ReferenceLineFilter OpenCV错误: %1").arg(ex.what()));
        ctx.reason = "参考线匹配失败";
    } catch (...) {
        qDebug() << "[ReferenceLineFilter] 未知异常";
    Logger::instance()->error(QString("ReferenceLineFilter 未知异常"));
        ctx.reason = "参考线匹配失败";
    }
}