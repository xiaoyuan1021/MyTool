#include "match_strategy.h"
#include "logger.h"
#include "image_utils.h"
#include <QCoreApplication>

MatchStrategy::MatchStrategy() {}

OpenCVMatchStrategy::OpenCVMatchStrategy()
    : m_hasTemplate(false)
    , m_matchMethod(cv::TM_CCOEFF_NORMED)
{
}

bool OpenCVMatchStrategy::createTemplate(const cv::Mat& fullImage,
                                         const QVector<QPointF>& polygon,
                                         const TemplateParams& params)
{
    if (fullImage.empty()) {
        Logger::instance()->error("[OpenCV] 创建模板失败：图像为空");
        return false;
    }

    if (polygon.size() < 3) {
        Logger::instance()->error("[OpenCV] 创建模板失败：多边形顶点数不足");
        return false;
    }

    try {
        // 1️⃣ 提取模板ROI
        m_templateImage = extractTemplateROI(fullImage, polygon);

        if (m_templateImage.empty()) {
            Logger::instance()->error("[OpenCV] 创建模板失败：ROI为空");
            return false;
        }

        // 2️⃣ 保存参数
        m_matchMethod = params.matchMethod;
        m_polygonPoints = polygon;

        m_hasTemplate = true;
        Logger::instance()->info(
            QString("✅ [OpenCV] 模板创建成功:  (尺寸: %1x%2, 方法: %3)")
                .arg(m_templateImage.cols)
                .arg(m_templateImage.rows)
                .arg(m_matchMethod)
            );
        return true;

    } catch (const cv::Exception& ex) {
        Logger::instance()->error(
            QString("[OpenCV] 创建模板失败: %1").arg(ex.what())
            );
        m_hasTemplate = false;
        return false;
    }
}

QVector<MatchResult> OpenCVMatchStrategy::findMatches(const cv::Mat& searchImage,
                                                      double minScore,
                                                      int maxMatches,
                                                      double greediness)
{
    QVector<MatchResult> results;

    if (searchImage.empty()) {
        Logger::instance()->error("[OpenCV] 匹配失败：搜索图像为空");
        return results;
    }

    if (!m_hasTemplate) {
        Logger::instance()->error("[OpenCV] 匹配失败：未创建模板");
        return results;
    }

    try {
        // 1️⃣ 确保图像和模板通道数一致
        cv::Mat searchGray, templateGray;
        
        if (searchImage.channels() == 3) {
            cv::cvtColor(searchImage, searchGray, cv::COLOR_BGR2GRAY);
        } else {
            searchGray = searchImage;
        }
        
        if (m_templateImage.channels() == 3) {
            cv::cvtColor(m_templateImage, templateGray, cv::COLOR_BGR2GRAY);
        } else {
            templateGray = m_templateImage;
        }

        // 2️⃣ 执行模板匹配
        cv::Mat matchResult;
        cv::matchTemplate(searchGray, templateGray, matchResult, m_matchMethod);

        // 3️⃣ 查找多个匹配点 - 优化实现
        // 对于TM_SQDIFF和TM_SQDIFF_NORMED，值越小越好
        bool isInverted = (m_matchMethod == cv::TM_SQDIFF ||
                           m_matchMethod == cv::TM_SQDIFF_NORMED);

        // 优化：使用阈值筛选代替循环查找
        // 计算阈值
        double threshold = minScore;
        if (m_matchMethod == cv::TM_CCOEFF ||
            m_matchMethod == cv::TM_CCORR ||
            m_matchMethod == cv::TM_SQDIFF) {
            // 对于非归一化方法，需要先归一化
            cv::normalize(matchResult, matchResult, 0, 1, cv::NORM_MINMAX);
        }
        
        // 创建二值化掩码
        cv::Mat mask;
        if (isInverted) {
            cv::threshold(matchResult, mask, 1.0 - threshold, 255, cv::THRESH_BINARY_INV);
        } else {
            cv::threshold(matchResult, mask, threshold, 255, cv::THRESH_BINARY);
        }
        mask.convertTo(mask, CV_8U);

        // 查找所有满足条件的点
        std::vector<cv::Point> locations;
        cv::findNonZero(mask, locations);
        
        // 按分数排序（从高到低）
        std::vector<std::pair<double, cv::Point>> scoredLocations;
        for (const auto& loc : locations) {
            double score = matchResult.at<float>(loc);
            if (isInverted) {
                score = 1.0 - score;
            }
            scoredLocations.push_back({score, loc});
        }
        
        // 降序排序
        std::sort(scoredLocations.begin(), scoredLocations.end(),
                  [](const auto& a, const auto& b) { return a.first > b.first; });
        
        // 非极大值抑制（NMS）
        int maskRadius = (std::max)(templateGray.cols, templateGray.rows) / 2;
        cv::Mat nmsMask = cv::Mat::ones(matchResult.size(), CV_8U) * 255;
        
        int foundCount = 0;
        for (const auto& [score, loc] : scoredLocations) {
            if (foundCount >= maxMatches) break;
            if (score < minScore) break;
            
            // 检查是否被NMS抑制
            if (nmsMask.at<uchar>(loc) == 0) continue;
            
            // 添加匹配结果
            MatchResult match;
            match.column = loc.x + templateGray.cols / 2.0;
            match.row = loc.y + templateGray.rows / 2.0;
            match.angle = 0.0;  // OpenCV标准模板匹配不支持旋转
            match.score = score;
            results.append(match);
            
            foundCount++;
            
            // NMS：屏蔽已找到的区域
            cv::circle(nmsMask, loc, maskRadius, cv::Scalar(0), -1);
        }

        Logger::instance()->info(
            QString("✅ [OpenCV] 找到 %1 个匹配 (最低分数: %2)")
                .arg(foundCount).arg(minScore)
            );

    } catch (const cv::Exception& ex) {
        Logger::instance()->error(
            QString("[OpenCV] 模板匹配失败: %1").arg(ex.what())
            );
    }

    return results;
}

cv::Mat OpenCVMatchStrategy::drawMatches(const cv::Mat& searchImage,
                                         const QVector<MatchResult>& matches) const
{
    if (searchImage.empty() || matches.isEmpty()) {
        return searchImage.clone();
    }

    cv::Mat result = searchImage.clone();
    if (result.channels() == 1) {
        cv::cvtColor(result, result, cv::COLOR_GRAY2BGR);
    }

    int halfWidth = m_templateImage.cols / 2;
    int halfHeight = m_templateImage.rows / 2;

    for (int i = 0; i < matches.size(); ++i) {
        // 根据匹配质量选择颜色
        cv::Scalar color;
        if (matches[i].score >= 0.8)
        {
            color = cv::Scalar(0, 255, 0);      // 绿色
        }
        else if (matches[i].score >= 0.6)
        {
            color = cv::Scalar(0, 255, 255);    // 黄色
        }
        else
        {
            color = cv::Scalar(0, 165, 255);    // 橙色
        }

        // 绘制矩形框
        cv::Point topLeft(matches[i].column - halfWidth,
                          matches[i].row - halfHeight);
        cv::Point bottomRight(matches[i].column + halfWidth,
                              matches[i].row + halfHeight);
        cv::rectangle(result, topLeft, bottomRight, color, 2);

        // 绘制中心点
        cv::Point center(matches[i].column, matches[i].row);
        cv::circle(result, center, 5, color, -1);
        cv::circle(result, center, 8, color, 2);

        // 绘制文字信息
        QString info = QString("#%1 Score:%2")
                           .arg(i + 1)
                           .arg(matches[i].score, 0, 'f', 2);
        cv::putText(result, info.toStdString(),
                    cv::Point(matches[i].column + 15, matches[i].row - 15),
                    cv::FONT_HERSHEY_SIMPLEX, 0.5, color, 2);
    }

    return result;
}

cv::Mat OpenCVMatchStrategy::extractTemplateROI(const cv::Mat& image,
                                                const QVector<QPointF>& polygon)
{
    // 1️⃣ 转换为OpenCV点
    std::vector<cv::Point> cvPolygon;
    for (const QPointF& pt : polygon) {
        cvPolygon.push_back(cv::Point(pt.x(), pt.y()));
    }

    // 2️⃣ 获取外接矩形
    cv::Rect boundingRect = cv::boundingRect(cvPolygon);

    // 3️⃣ 确保矩形在图像范围内
    boundingRect &= cv::Rect(0, 0, image.cols, image.rows);//运算符重载

    if (boundingRect.width <= 0 || boundingRect.height <= 0) {
        return cv::Mat();
    }

    // 4️⃣ 提取ROI
    cv::Mat roi = image(boundingRect).clone();

    // 5️⃣ 创建掩码（可选：如果需要精确的多边形区域）
    // 这里简化处理，直接返回矩形ROI
    // 如果需要多边形掩码，可以使用cv::fillPoly

    return roi;
}
