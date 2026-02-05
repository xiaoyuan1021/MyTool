#include "match_strategy.h"
#include "logger.h"
#include "image_utils.h"

match_strategy::match_strategy() {}

ShapeMatchStrategy::ShapeMatchStrategy()
    :m_hasTemplate(false)
    ,m_modelRow(0)
    ,m_modelCol(0)
{

}

bool ShapeMatchStrategy::createTemplate(const Mat &fullImage, const QVector<QPointF>& pologonPoints, const TemplateParams &params)
{
    if (fullImage.empty())
    {
        Logger::instance()->error("[Shape] 创建模板失败：图像为空");
        return false;
    }

    if (pologonPoints.size() < 3)
    {
        Logger::instance()->error("[Shape] 创建模板失败：多边形顶点数不足");
        return false;
    }
    try
    {
        HImage templateRegion =createTemplateRegion(fullImage,pologonPoints);
        m_model.CreateShapeModel(templateRegion,
                                 params.numLevels,
                                 params.angleStart,
                                 params.angleExtent,
                                 params.angleStep,
                                 params.optimization.toStdString().c_str(),
                                 params.metric.toStdString().c_str(),
                                 "auto",
                                 "auto");
        m_model.GetShapeModelOrigin(&m_modelRow, &m_modelCol);
        std::vector<cv::Point> cvPolygon;
        for(const QPointF& pt : pologonPoints)
        {
            cvPolygon.push_back(cv::Point(pt.x(),pt.y()));//?
        }
        cv::Rect boundingRect = cv::boundingRect(cvPolygon);
        m_templateImage =fullImage(boundingRect).clone();

        m_polygonPoints =pologonPoints;
        extractTemplateContour(pologonPoints);//有啥用
        m_hasTemplate =true;
        Logger::instance()->info(
            QString("✅ [Shape] 模板创建成功:"));
        return true;
    }
    catch (const HException& ex)
    {
        Logger::instance()->info(QString("[Shape] 创建模板失败: %1")
                                     .arg(ex.ErrorMessage().Text()));
        m_hasTemplate =false;
        return false;
    }
}

QVector<MatchResult> ShapeMatchStrategy::findMatches(const Mat &searchImage, double minScore, int maxMatches, double greediness)
{
    QVector<MatchResult> result;
    if(searchImage.empty())
    {
        Logger::instance()->error("[Shape] 匹配失败：搜索图像为空");
        return result;
    }
    if(!m_hasTemplate)
    {
        Logger::instance()->error("[Shape] 匹配失败：未创建模板");
        return result;
    }
    try
    {
        HImage searchHImage =ImageUtils::Mat2HImage(searchImage);
        HTuple row, column, angle, score;
        m_model.FindShapeModel(searchHImage,
                               0,
                               6.28318,
                               minScore,
                               maxMatches,
                               0.5,
                               "least_squares",
                               0,
                               greediness,
                               &row,&column,&angle,&score
                               );
        int numFound = row.Length();
        for(int i = 0; i<numFound; ++i)
        {
            MatchResult match;
            match.row = row[i].D();
            match.column = column[i].D();
            match.angle = angle[i].D() * 57.2958;// 弧度转角度
            match.score = score[i].D();
            result.append(match);
        }
        Logger::instance()->info(QString("✅ [Shape] 找到 %1 个匹配 (最低分数: %2)")
                                 .arg(numFound).arg(minScore));
        return result;

    }
    catch (const HException& ex)
    {
        Logger::instance()->error(
            QString("[Shape] 模板匹配失败: %1").arg(ex.ErrorMessage().Text()));
        return result;
    }
}

Mat ShapeMatchStrategy::drawMatches(const Mat &searchImage, const QVector<MatchResult> &matches) const
{
    if (searchImage.empty() || matches.isEmpty()) {
        return searchImage;
    }

    cv::Mat result = searchImage.clone();
    if (result.channels() == 1) {
        cv::cvtColor(result, result, cv::COLOR_GRAY2BGR);
    }

    // 为每个匹配绘制轮廓
    for (int i = 0; i < matches.size(); ++i)
    {
        // 根据匹配质量选择颜色
        cv::Scalar color;
        if (matches[i].score >= 0.8)
        {
            color = cv::Scalar(0, 255, 0);      // 绿色 - 高质量
        }
        else if (matches[i].score >= 0.6)
        {
            color = cv::Scalar(0, 255, 255);    // 黄色 - 中等质量
        }
        else
        {
            color = cv::Scalar(0, 165, 255);    // 橙色 - 低质量
        }

        drawSingleMatch(result, matches[i], color);

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

HImage ShapeMatchStrategy::createTemplateRegion(const Mat &image, const QVector<QPointF> &polygon)
{
    // 1️⃣ 转换为 HImage
    HImage hImage = ImageUtils::Mat2HImage(image);

    // 2️⃣ 创建多边形 Region
    HTuple rows, cols;
    for (const QPointF& pt : polygon) {
        rows.Append(pt.y());
        cols.Append(pt.x());
    }

    HRegion polygonRegion;
    polygonRegion.GenRegionPolygon(rows, cols);

    // 3️⃣ 裁剪图像
    return hImage.ReduceDomain(polygonRegion);
}

void ShapeMatchStrategy::extractTemplateContour(const QVector<QPointF> &polygon)
{
    try {
        // 1️⃣ 创建多边形轮廓
        HTuple rows, cols;
        for (const QPointF& pt : polygon) {
            rows.Append(pt.y());
            cols.Append(pt.x());
        }

        // 2️⃣ 生成轮廓Region
        m_templateContour.GenRegionPolygon(rows, cols);

        // 3️⃣ 获取轮廓边界点
        HRegion boundary = m_templateContour.Boundary("inner");
        boundary.GetRegionPoints(&m_templateRows, &m_templateCols);

        // 如果边界点太少，使用原始多边形点
        if (m_templateRows.Length() < 4) {
            m_templateRows = rows;
            m_templateCols = cols;
        }

    } catch (const HException& ex) {
        Logger::instance()->warning(
            QString("[Shape] 提取模板轮廓失败: %1").arg(ex.ErrorMessage().Text())
            );
        // 使用原始多边形作为后备
        HTuple rows, cols;
        for (const QPointF& pt : polygon) {
            rows.Append(pt.y());
            cols.Append(pt.x());
        }
        m_templateRows = rows;
        m_templateCols = cols;
    }
}

void ShapeMatchStrategy::drawSingleMatch(Mat &image, const MatchResult &match, const Scalar &color) const
{
    try {
        // 📖 仿射变换原理：
        // 1. 平移到原点（-modelRow, -modelCol）
        // 2. 旋转（match.angle）
        // 3. 平移到匹配位置（match.row, match.column）

        HTuple homMat2D;
        HomMat2dIdentity(&homMat2D);
        HomMat2dTranslate(homMat2D, -m_modelRow, -m_modelCol, &homMat2D);
        HomMat2dRotate(homMat2D, match.angle * 0.0174533, 0, 0, &homMat2D);
        HomMat2dTranslate(homMat2D, match.row, match.column, &homMat2D);

        // 对轮廓点应用仿射变换
        HTuple transformedRows, transformedCols;
        AffineTransPoint2d(homMat2D,
                           m_templateRows,
                           m_templateCols,
                           &transformedRows,
                           &transformedCols);

        // 转换为OpenCV点并绘制
        std::vector<cv::Point> contourPoints;
        for (int i = 0; i < transformedRows.Length(); ++i) {
            contourPoints.push_back(
                cv::Point(transformedCols[i].D(), transformedRows[i].D())
                );
        }

        // 绘制填充多边形（半透明）
        if (contourPoints.size() >= 3) {
            cv::Mat overlay = image.clone();
            cv::fillPoly(overlay, contourPoints, color);
            cv::addWeighted(overlay, 0.3, image, 0.7, 0, image);

            // 绘制轮廓线
            cv::polylines(image, contourPoints, true, color, 2);
        }

    } catch (const HException& ex) {
        Logger::instance()->warning(
            QString("[Shape] 绘制匹配轮廓失败: %1").arg(ex.ErrorMessage().Text())
            );
        // 降级方案：绘制简单矩形
        cv::Rect rect(match.column - 50, match.row - 50, 100, 100);
        cv::rectangle(image, rect, color, 2);
    }
}

NCCMatchStrategy::NCCMatchStrategy()
    : m_hasTemplate(false)
    , m_templateWidth(0)
    , m_templateHeight(0)
{
}

bool NCCMatchStrategy::createTemplate(const cv::Mat& fullImage,
                                      const QVector<QPointF>& polygon,
                                      const TemplateParams& params)
{
    if (fullImage.empty()) {
        Logger::instance()->error("[NCC] 创建模板失败：图像为空");
        return false;
    }

    if (polygon.size() < 3) {
        Logger::instance()->error("[NCC] 创建模板失败：多边形顶点数不足");
        return false;
    }

    try {
        // 1️⃣ 创建模板区域
        HImage templateRegion = createTemplateRegion(fullImage, polygon);

        // 2️⃣ 创建 NCC Model
        m_model.CreateNccModel(
            templateRegion,
            params.nccLevels,                // NumLevels (0 = auto)
            params.angleStart * 0.0174533,   // AngleStart
            params.angleExtent * 0.0174533,  // AngleExtent
            params.angleStep * 0.0174533,    // AngleStep
            params.metric.toStdString().c_str()  // Metric
            );

        // 3️⃣ 保存模板图像
        std::vector<cv::Point> cvPolygon;
        for (const QPointF& pt : polygon) {
            cvPolygon.push_back(cv::Point(pt.x(), pt.y()));
        }
        cv::Rect boundingRect = cv::boundingRect(cvPolygon);
        m_templateImage = fullImage(boundingRect).clone();

        m_templateWidth = boundingRect.width;
        m_templateHeight = boundingRect.height;
        m_polygonPoints = polygon;

        m_hasTemplate = true;
        Logger::instance()->info(
            QString("✅ [NCC] 模板创建成功:  (尺寸: %1x%2)")
                .arg(m_templateWidth)
                .arg(m_templateHeight)
            );
        return true;

    } catch (const HException& ex) {
        Logger::instance()->error(
            QString("[NCC] 创建模板失败: %1").arg(ex.ErrorMessage().Text())
            );
        m_hasTemplate = false;
        return false;
    }
}

QVector<MatchResult> NCCMatchStrategy::findMatches(const cv::Mat& searchImage,
                                                   double minScore,
                                                   int maxMatches,
                                                   double greediness)
{
    QVector<MatchResult> results;

    if (searchImage.empty()) {
        Logger::instance()->error("[NCC] 匹配失败：搜索图像为空");
        return results;
    }

    if (!m_hasTemplate) {
        Logger::instance()->error("[NCC] 匹配失败：未创建模板");
        return results;
    }

    try {
        // 1️⃣ 转换搜索图像
        HImage searchHImage = ImageUtils::Mat2HImage(searchImage);

        // 2️⃣ 查找模板
        HTuple row, column, angle, score;

        m_model.FindNccModel(
            searchHImage,
            0,                    // AngleStart
            6.28318,              // AngleExtent (2π)
            minScore,             // MinScore
            maxMatches,           // NumMatches
            0.5,                  // MaxOverlap
            "true",               // SubPixel
            0,                    // NumLevels (0=all)
            &row, &column, &angle, &score
            );

        // 3️⃣ 解析结果
        int numFound = row.Length();
        for (int i = 0; i < numFound; ++i) {
            MatchResult match;
            match.row = row[i].D();
            match.column = column[i].D();
            match.angle = angle[i].D() * 57.2958;  // 弧度转角度
            match.score = score[i].D();
            results.append(match);
        }

        Logger::instance()->info(
            QString("✅ [NCC] 找到 %1 个匹配 (最低分数: %2)")
                .arg(numFound).arg(minScore)
            );

    } catch (const HException& ex) {
        Logger::instance()->error(
            QString("[NCC] 模板匹配失败: %1").arg(ex.ErrorMessage().Text())
            );
    }

    return results;
}

cv::Mat NCCMatchStrategy::drawMatches(const cv::Mat& searchImage,
                                      const QVector<MatchResult>& matches) const
{
    if (searchImage.empty() || matches.isEmpty()) {
        return searchImage.clone();
    }

    cv::Mat result = searchImage.clone();
    if (result.channels() == 1) {
        cv::cvtColor(result, result, cv::COLOR_GRAY2BGR);
    }

    // NCC不支持轮廓绘制，使用矩形框
    for (int i = 0; i < matches.size(); ++i) {
        // 根据匹配质量选择颜色
        cv::Scalar color;
        if (matches[i].score >= 0.8) {
            color = cv::Scalar(0, 255, 0);      // 绿色
        } else if (matches[i].score >= 0.6) {
            color = cv::Scalar(0, 255, 255);    // 黄色
        } else {
            color = cv::Scalar(0, 165, 255);    // 橙色
        }

        // 计算旋转矩形的四个顶点
        double halfWidth = m_templateWidth / 2.0;
        double halfHeight = m_templateHeight / 2.0;
        double angleRad = matches[i].angle * 0.0174533;
        double cosA = cos(angleRad);
        double sinA = sin(angleRad);

        // 四个角点（未旋转）
        cv::Point2f corners[4] = {
            cv::Point2f(-halfWidth, -halfHeight),
            cv::Point2f(halfWidth, -halfHeight),
            cv::Point2f(halfWidth, halfHeight),
            cv::Point2f(-halfWidth, halfHeight)
        };

        // 旋转并平移
        std::vector<cv::Point> rotatedCorners;
        for (int j = 0; j < 4; ++j) {
            double x = corners[j].x * cosA - corners[j].y * sinA + matches[i].column;
            double y = corners[j].x * sinA + corners[j].y * cosA + matches[i].row;
            rotatedCorners.push_back(cv::Point(x, y));
        }

        // 绘制旋转矩形
        cv::polylines(result, rotatedCorners, true, color, 2);

        // 绘制中心点
        cv::Point center(matches[i].column, matches[i].row);
        cv::circle(result, center, 5, color, -1);
        cv::circle(result, center, 8, color, 2);

        // 绘制文字信息
        QString info = QString("#%1 Score:%2 Angle:%3°")
                           .arg(i + 1)
                           .arg(matches[i].score, 0, 'f', 2)
                           .arg(matches[i].angle, 0, 'f', 1);
        cv::putText(result, info.toStdString(),
                    cv::Point(matches[i].column + 15, matches[i].row - 15),
                    cv::FONT_HERSHEY_SIMPLEX, 0.5, color, 2);
    }

    return result;
}

HImage NCCMatchStrategy::createTemplateRegion(const cv::Mat& image,
                                              const QVector<QPointF>& polygon)
{
    // 1️⃣ 转换为 HImage
    HImage hImage = ImageUtils::Mat2HImage(image);

    // 2️⃣ 创建多边形 Region
    HTuple rows, cols;
    for (const QPointF& pt : polygon) {
        rows.Append(pt.y());
        cols.Append(pt.x());
    }

    HRegion polygonRegion;
    polygonRegion.GenRegionPolygon(rows, cols);

    // 3️⃣ 裁剪图像
    return hImage.ReduceDomain(polygonRegion);
}


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
        cv::Mat searchGray = searchImage.clone();
        cv::Mat templateGray = m_templateImage.clone();

        if (searchGray.channels() == 3) {
            cv::cvtColor(searchGray, searchGray, cv::COLOR_BGR2GRAY);
        }
        if (templateGray.channels() == 3) {
            cv::cvtColor(templateGray, templateGray, cv::COLOR_BGR2GRAY);
        }

        // 2️⃣ 执行模板匹配
        cv::Mat matchResult;
        cv::matchTemplate(searchGray, templateGray, matchResult, m_matchMethod);

        // 3️⃣ 查找多个匹配点
        // OpenCV的matchTemplate返回整个相似度图，需要手动查找峰值

        // 对于TM_SQDIFF和TM_SQDIFF_NORMED，值越小越好
        bool isInverted = (m_matchMethod == cv::TM_SQDIFF ||
                           m_matchMethod == cv::TM_SQDIFF_NORMED);

        // 归一化到[0,1]
        cv::Mat normalizedResult;
        if (m_matchMethod == cv::TM_CCOEFF ||
            m_matchMethod == cv::TM_CCORR ||
            m_matchMethod == cv::TM_SQDIFF) {
            cv::normalize(matchResult, normalizedResult, 0, 1, cv::NORM_MINMAX);
        } else {
            normalizedResult = matchResult.clone();
        }

        // 查找多个局部极值点
        int foundCount = 0;
        cv::Mat mask = cv::Mat::ones(normalizedResult.size(), CV_8U) * 255;

        for (int i = 0; i < maxMatches && foundCount < maxMatches; ++i) {
            double minVal, maxVal;
            cv::Point minLoc, maxLoc;
            cv::minMaxLoc(normalizedResult, &minVal, &maxVal, &minLoc, &maxLoc, mask);

            cv::Point matchLoc = isInverted ? minLoc : maxLoc;
            double score = isInverted ? (1.0 - minVal) : maxVal;

            // 检查分数是否满足阈值
            if (score < minScore) {
                break;
            }

            // 添加匹配结果
            MatchResult match;
            match.column = matchLoc.x + templateGray.cols / 2.0;
            match.row = matchLoc.y + templateGray.rows / 2.0;
            match.angle = 0.0;  // OpenCV标准模板匹配不支持旋转
            match.score = score;
            results.append(match);

            foundCount++;

            // 屏蔽已找到的区域（避免重复检测）
            int maskRadius = std::max(templateGray.cols, templateGray.rows) / 2;
            cv::circle(mask, matchLoc, maskRadius, cv::Scalar(0), -1);
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
        if (matches[i].score >= 0.8) {
            color = cv::Scalar(0, 255, 0);      // 绿色
        } else if (matches[i].score >= 0.6) {
            color = cv::Scalar(0, 255, 255);    // 黄色
        } else {
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
    boundingRect &= cv::Rect(0, 0, image.cols, image.rows);

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
