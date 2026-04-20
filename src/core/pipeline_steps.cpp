#include "pipeline_steps.h"
#include "opencv_algorithm.h"
#include <opencv2/line_descriptor.hpp>
#include <cmath>
#include <algorithm>

void StepColorChannel::run(PipelineContext &ctx)
{
    if (ctx.srcBgr.empty() || !cfg_) return;
    switch (cfg_->channel)
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
    // 如果不是灰度过滤模式，释放mask并返回
    if (cfg_->currentFilterMode != ImageFilterMode::Gray)
    {
        ctx.mask.release();
        return;
    }

    if (!cfg_ || !cfg_->enableGrayFilter)
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

        // 使用OpenCV的inRange替代Halcon的Threshold
        // 灰度范围 [grayLow, grayHigh] 内的像素设为255（绿色/目标），其余为0（白色/背景）
        cv::inRange(gray, cv::Scalar(cfg_->grayLow), cv::Scalar(cfg_->grayHigh), ctx.mask);

        ctx.reason = QString("灰度过滤: 范围[%1,%2]")
                         .arg(cfg_->grayLow)
                         .arg(cfg_->grayHigh);
    }
    catch (const cv::Exception& ex)
    {
        qDebug() << "OpenCV灰度过滤错误:" << ex.what();
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
        // ✅ 修改后：优先使用processed，因为算法队列的结果存储在processed中
        cv::Mat inputMat = !ctx.processed.empty() ? ctx.processed : ctx.mask;
        
        if (!m_cfg || inputMat.empty()) {
            return;
        }

        const ShapeFilterConfig& filter = m_cfg->shapeFilter;

        if (!filter.hasValidConditions()) {
            return;
        }

        try {
            // 使用OpenCV进行连通域分析
            cv::Mat binary;
            if (inputMat.channels() == 3) {
                cv::cvtColor(inputMat, binary, cv::COLOR_BGR2GRAY);
            } else {
                binary = inputMat.clone();
            }
            cv::threshold(binary, binary, 127, 255, cv::THRESH_BINARY);

            cv::Mat labels, stats, centroids;
            int numBefore = cv::connectedComponentsWithStats(binary, labels, stats, centroids, 8) - 1; // 减去背景

            qDebug() << "========== 形状筛选 ==========";
            qDebug() << "筛选模式:" << getFilterModeName(filter.mode);
            qDebug() << "筛选前区域数量:" << numBefore;

            // 应用筛选
            cv::Mat filteredRegion = applyFilter(binary, filter);

            // 统计筛选后的区域数量
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

            // 使用OpenCV的selectShapeByFeature函数
            result = OpenCVAlgorithm::selectShapeByFeature(
                result,
                QString(getFeatureName(cond.feature)),
                cond.minValue,
                cond.maxValue
            );

            // 统计剩余区域数量
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

            // 统计该条件匹配的区域数量
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
        // 如果是灰度过滤模式，不处理（让StepGrayFilter处理mask）
        if (m_cfg->currentFilterMode == ImageFilterMode::Gray) {
            return;
        }
        
        // 如果不是颜色过滤模式（RGB/HSV），释放mask并返回
        if (m_cfg->currentFilterMode != ImageFilterMode::RGB &&
            m_cfg->currentFilterMode != ImageFilterMode::HSV) {
            ctx.mask.release();
            return;
        }

        if (!m_cfg->enableColorFilter) {
            ctx.mask.release();
            return;
        }

        cv::Mat input = ctx.enhanced.empty() ? ctx.srcBgr : ctx.enhanced;

        cv::Mat filterMask;
        switch (m_cfg->colorFilterMode)
        {
        case ColorFilterMode::RGB:
            filterMask = m_processor->filterRGB(
                input,
                m_cfg->rLow, m_cfg->rHigh,
                m_cfg->gLow, m_cfg->gHigh,
                m_cfg->bLow, m_cfg->bHigh
                );
            break;

        case ColorFilterMode::HSV:
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

        cv::bitwise_not(filterMask,ctx.mask);

        ctx.reason = QString("颜色过滤: %1 模式")
                         .arg(m_cfg->colorFilterMode == ColorFilterMode::RGB ? "RGB" : "HSV");
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
        else if (cfg_->lineDetectAlgorithm == 3)
        {
            // ⚠️ EdgesSubPix 功能已禁用（依赖 Halcon）
            qDebug() << "[LineDetect] EdgesSubPix 算法已禁用，请选择其他算法";
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
        // EdgesSubPix用端点连线
        else if (cfg_->lineDetectAlgorithm == 3)
        {
            for (const auto& line : lines)
            {
                cv::line(ctx.lineDetect,
                        cv::Point(line[0], line[1]),
                        cv::Point(line[2], line[3]),
                        cv::Scalar(0, 255, 0), 2);
            }
        }
    }

// ========== 参考线匹配辅助函数 ==========

/**
 * 计算直线角度（与X轴正方向的夹角，范围-180到180度）
 */
static double calculateLineAngle(const cv::Vec4f& line)
{
    cv::Point2f p1(line[0], line[1]);
    cv::Point2f p2(line[2], line[3]);
    double angle = std::atan2(p2.y - p1.y, p2.x - p1.x) * 180.0 / CV_PI;
    return angle;
}

/**
 * 计算点到直线的距离
 * @param point 目标点
 * @param lineStart 直线起点
 * @param lineEnd 直线终点
 * @return 点到直线的距离
 */
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

/**
 * 计算两条直线的角度差（考虑方向性）
 */
static double calculateAngleDifference(double angle1, double angle2)
{
    double diff = std::abs(angle1 - angle2);
    // 处理角度环绕（如170度和-170度相差20度而非340度）
    if (diff > 180.0) {
        diff = 360.0 - diff;
    }
    return diff;
}

/**
 * 检测直线是否与参考线匹配
 */
static bool isLineMatchingReference(const cv::Vec4f& line,
                                    const cv::Point2f& refStart,
                                    const cv::Point2f& refEnd,
                                    double refAngle,
                                    double angleThreshold,
                                    double distanceThreshold)
{
    // 计算检测直线的角度
    double lineAngle = calculateLineAngle(line);
    double angleDiff = calculateAngleDifference(lineAngle, refAngle);
    
    // 角度过滤
    if (angleDiff > angleThreshold) {
        return false;
    }
    
    // 计算检测直线的中点到参考线的距离
    cv::Point2f lineMid((line[0] + line[2]) / 2.0f, (line[1] + line[3]) / 2.0f);
    double dist = pointToLineDistance(lineMid, refStart, refEnd);
    
    // 距离过滤
    if (dist > distanceThreshold) {
        return false;
    }
    
    return true;
}

/**
 * 计算参考线角度
 */
static double calculateReferenceLineAngle(const PipelineConfig* cfg)
{
    return std::atan2(cfg->referenceLineEnd.y - cfg->referenceLineStart.y,
                      cfg->referenceLineEnd.x - cfg->referenceLineStart.x) * 180.0 / CV_PI;
}

// ========== 参考线匹配步骤 ==========

void StepReferenceLineFilter::run(PipelineContext& ctx)
{
    if (!cfg_ || !cfg_->enableReferenceLineMatch) {
        return;
    }
    
    if (!cfg_->referenceLineValid) {
        ctx.reason = "参考线未绘制";
        return;
    }
    
    if (ctx.srcBgr.empty()) {
        return;
    }
    
    cv::Mat src = ctx.srcBgr;
    
    // 计算参考线包围的搜索区域
    cv::Point2f refStart = cfg_->referenceLineStart;
    cv::Point2f refEnd = cfg_->referenceLineEnd;
    
    // 计算参考线的法向量
    cv::Point2f lineDir = refEnd - refStart;
    double lineLength = cv::norm(lineDir);
    if (lineLength < 1e-6) {
        ctx.reason = "参考线长度太短";
        return;
    }
    
    cv::Point2f normal(-lineDir.y / lineLength, lineDir.x / lineLength);
    
    // 计算搜索区域的四个角点
    double halfWidth = cfg_->searchRegionWidth / 2.0;
    cv::Point2f offset = normal * halfWidth;
    
    std::vector<cv::Point> searchRegion = {
        cv::Point(refStart.x + offset.x, refStart.y + offset.y),
        cv::Point(refEnd.x + offset.x, refEnd.y + offset.y),
        cv::Point(refEnd.x - offset.x, refEnd.y - offset.y),
        cv::Point(refStart.x - offset.x, refStart.y - offset.y)
    };
    
    // 创建搜索区域的掩码
    cv::Mat regionMask = cv::Mat::zeros(src.size(), CV_8UC1);
    std::vector<std::vector<cv::Point>> contours = {searchRegion};
    cv::fillPoly(regionMask, contours, cv::Scalar(255));
    
    // 只在搜索区域内进行直线检测
    cv::Mat gray;
    cv::cvtColor(src, gray, cv::COLOR_BGR2GRAY);
    
    cv::Mat maskedGray;
    gray.copyTo(maskedGray, regionMask);
    
    cv::Mat edges;
    cv::Canny(maskedGray, edges, 50, 150);
    
    std::vector<cv::Vec4f> allLines;
    
    // 根据算法检测直线
    if (cfg_->lineDetectAlgorithm == 0) {
        detectLinesHoughP(edges, cfg_, allLines);
    } else if (cfg_->lineDetectAlgorithm == 1) {
        detectLinesLSD(maskedGray, allLines);
    } else if (cfg_->lineDetectAlgorithm == 2) {
        detectLinesEDlines(maskedGray, allLines);
    } else if (cfg_->lineDetectAlgorithm == 3) {
        // ⚠️ EdgesSubPix 功能已禁用（依赖 Halcon）
        qDebug() << "[ReferenceLineFilter] EdgesSubPix 算法已禁用，请选择其他算法";
    }
    
    // 计算参考线角度
    double refAngle = calculateReferenceLineAngle(cfg_);
    
    // 筛选匹配的直线
    std::vector<cv::Vec4f> matchedLines;
    for (const auto& line : allLines) {
        if (isLineMatchingReference(line,
                                    cfg_->referenceLineStart,
                                    cfg_->referenceLineEnd,
                                    refAngle,
                                    cfg_->angleThreshold,
                                    cfg_->distanceThreshold)) {
            matchedLines.push_back(line);
        }
    }
    
    // 绘制结果
    ctx.lineDetect = src.clone();
    
    // 绘制参考线（醒目的黄色粗线）
    cv::line(ctx.lineDetect,
             cfg_->referenceLineStart,
             cfg_->referenceLineEnd,
             cv::Scalar(0, 255, 255), 4, cv::LINE_AA);
    
    // 只显示第一个匹配的直线（最匹配的直线）
    if (!matchedLines.empty()) {
        const auto& line = matchedLines[0];
        cv::line(ctx.lineDetect,
                 cv::Point(line[0], line[1]),
                 cv::Point(line[2], line[3]),
                 cv::Scalar(255, 0, 255), 3, cv::LINE_AA);
    }
    
    // 绘制不匹配的直线（绿色，较细）
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
                     .arg(cfg_->angleThreshold)
                     .arg(cfg_->distanceThreshold);
    
    // 设置匹配结果到上下文
    ctx.matchedLineCount = static_cast<int>(matchedLines.size());
    ctx.totalLineCount = static_cast<int>(allLines.size());
    
    qDebug() << "[ReferenceLineFilter]" << ctx.reason;
}
