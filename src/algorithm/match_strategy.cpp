#include "match_strategy.h"
#include "logger.h"
#include "image_utils.h"
#include <QCoreApplication>

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

    // 检查多边形是否足够大
    std::vector<cv::Point> cvPolygon;
    for(const QPointF& pt : pologonPoints)
    {
        cvPolygon.push_back(cv::Point(pt.x(), pt.y()));
    }
    cv::Rect boundingRect = cv::boundingRect(cvPolygon);

    // 检查模板区域大小（至少 20x20 像素）
    if (boundingRect.width < 20 || boundingRect.height < 20)
    {
        Logger::instance()->error(QString("[Shape] 创建模板失败：模板区域过小 (%1x%2)，建议至少 20x20 像素")
                                     .arg(boundingRect.width).arg(boundingRect.height));
        return false;
    }

    try
    {
        HImage templateRegion = createTemplateRegion(fullImage, pologonPoints);

        // 检查提取的模板区域是否有效
        Hlong regionWidth = 0, regionHeight = 0;
        templateRegion.GetImageSize(&regionWidth, &regionHeight);

        if (regionWidth < 10 || regionHeight < 10)
        {
            Logger::instance()->error(QString("[Shape] 创建模板失败：提取的模板区域过小 (%1x%2)")
                                         .arg(regionWidth).arg(regionHeight));
            return false;
        }

        Logger::instance()->info(QString("[Shape] 模板区域大小: %1x%2").arg(regionWidth).arg(regionHeight));

        // 关键：使用 "point_reduction_low" 而不是 "auto" 或 "none"
        // "auto" 会自动选择激进的优化策略（如 point_reduction_high）导致特征点不足
        // "none" 保留所有特征点，但可能导致过度拟合，匹配分数低
        // "point_reduction_low" 是平衡方案：保留足够特征点，又不过度优化
        m_model.CreateShapeModel(templateRegion,
                                 params.numLevels,
                                 params.angleStart,
                                 params.angleExtent,
                                 params.angleStep,
                                 "point_reduction_low",  // 温和的优化
                                 params.metric.toStdString().c_str(),
                                 "auto",
                                 "auto");

        // 诊断：检查模型中的特征点数量
        double angleStart, angleExtent, angleStep, scaleMin, scaleMax, scaleStep;
        HString metric;
        Hlong minContrast;
        Hlong numLevels = m_model.GetShapeModelParams(&angleStart, &angleExtent, &angleStep,
                                                       &scaleMin, &scaleMax, &scaleStep,
                                                       &metric, &minContrast);
        Logger::instance()->info(QString("[Shape] Shape Model 创建成功，金字塔层数: %1").arg(numLevels));
        Logger::instance()->info(QString("[Shape] 角度范围: [%1, %2], 步长: %3").arg(angleStart).arg(angleExtent).arg(angleStep));
        Logger::instance()->info(QString("[Shape] 最小对比度: %1").arg(minContrast));

        // 使用 InspectShapeModel 可视化特征点
        try {
            HRegion modelRegions;
            HImage inspectImage = templateRegion.InspectShapeModel(&modelRegions, numLevels, 128);

            Logger::instance()->info("[Shape] InspectShapeModel 执行成功");

            // 检查 inspectImage 是否有效
            Hlong inspectWidth = 0, inspectHeight = 0;
            inspectImage.GetImageSize(&inspectWidth, &inspectHeight);
            Logger::instance()->info(QString("[Shape] InspectShapeModel 输出图像大小: %1x%2").arg(inspectWidth).arg(inspectHeight));

            // 将检查结果保存为调试图像
            HObject hObj = inspectImage;
            cv::Mat debugMat = ImageUtils::HImageToMat(hObj);
            Logger::instance()->info(QString("[Shape] HImageToMat 转换后 Mat 大小: %1x%2, 通道数: %3, 是否为空: %4")
                                         .arg(debugMat.cols).arg(debugMat.rows).arg(debugMat.channels()).arg(debugMat.empty()));

            if (!debugMat.empty()) {
                QString debugPath = QCoreApplication::applicationDirPath() + "/../../debug_shape_model.png";
                bool writeSuccess = cv::imwrite(debugPath.toStdString(), debugMat);
                Logger::instance()->info(QString("[Shape] 模型检查图像保存 %1: %2")
                                             .arg(writeSuccess ? "成功" : "失败").arg(debugPath));
            } else {
                Logger::instance()->warning("[Shape] debugMat 为空，无法保存");
            }
        } catch (const HException& ex) {
            Logger::instance()->warning(QString("[Shape] InspectShapeModel 失败: %1").arg(ex.ErrorMessage().Text()));
        } catch (const std::exception& ex) {
            Logger::instance()->warning(QString("[Shape] 诊断过程异常: %1").arg(ex.what()));
        }

        m_model.GetShapeModelOrigin(&m_modelRow, &m_modelCol);

        m_templateImage = fullImage(boundingRect).clone();

        // 保存 ROI 偏移量和尺寸（用于坐标转换）
        m_roiOffsetX = boundingRect.x;
        m_roiOffsetY = boundingRect.y;
        m_templateWidth = boundingRect.width;
        m_templateHeight = boundingRect.height;

        Logger::instance()->info(QString("[Shape] ShapeModel 原点: row=%1, col=%2").arg(m_modelRow).arg(m_modelCol));
        Logger::instance()->info(QString("[Shape] ROI 偏移量: x=%1, y=%2").arg(m_roiOffsetX).arg(m_roiOffsetY));

        m_polygonPoints = pologonPoints;
        extractTemplateContour(pologonPoints);
        m_hasTemplate = true;
        Logger::instance()->info(
            QString("✅ [Shape] 模板创建成功 (区域: %1x%2, 顶点: %3)")
                .arg(boundingRect.width)
                .arg(boundingRect.height)
                .arg(pologonPoints.size()));
        return true;
    }
    catch (const HException& ex)
    {
        Logger::instance()->error(QString("[Shape] 创建模板失败: %1")
                                     .arg(ex.ErrorMessage().Text()));
        m_hasTemplate = false;
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

    QImage qImg = ImageUtils::Mat2Qimage(result);

    QPainter painter(&qImg);
    painter.setRenderHint(QPainter::Antialiasing, true);

    // 为每个匹配绘制轮廓
    for (int i = 0; i < matches.size(); ++i)
    {
        // 根据匹配质量选择颜色
        QColor color;
        if (matches[i].score >= 0.8)
        {
            color = QColor(0, 255, 0);      // 绿色 - 高质量
        }
        else if (matches[i].score >= 0.6)
        {
            color = QColor(255, 255, 0);    // 黄色 - 中等质量
        }
        else
        {
            color = QColor(255, 165, 0);    // 橙色 - 低质量
        }

        // 绘制轮廓
        drawSingleMatch(painter, matches[i], color);

        // 绘制分数信息（放在轮廓外侧，避免重合）
        QString info = QString("#%1 Score:%2")
                           .arg(i + 1)
                           .arg(matches[i].score, 0, 'f', 2);
        painter.setPen(color);
        QFont font("Arial", 10, QFont::Bold);
        painter.setFont(font);
        painter.drawText(QPointF(matches[i].column + 20, matches[i].row - 20), info);
    }
    painter.end();
    cv::Mat results = ImageUtils::Qimage2Mat(qImg, true);
    return results;
}

HImage ShapeMatchStrategy::createTemplateRegion(const Mat &image, const QVector<QPointF> &polygon)
{
    // 诊断：输入图像大小
    Logger::instance()->info(QString("[Shape] createTemplateRegion 输入图像大小: %1x%2").arg(image.cols).arg(image.rows));
    Logger::instance()->info(QString("[Shape] 多边形顶点数: %1").arg(polygon.size()));

    if (polygon.size() < 3) {
        Logger::instance()->error("[Shape] 多边形顶点数不足");
        return ImageUtils::Mat2HImage(image);
    }

    // 打印多边形坐标
    for (int i = 0; i < polygon.size(); ++i) {
        Logger::instance()->info(QString("[Shape] 顶点 %1: (%2, %3)").arg(i).arg(polygon[i].x()).arg(polygon[i].y()));
    }

    // 关键修复：使用矩形 ROI 而不是多边形裁剪
    // 原因：多边形坐标是相对于完整图像的，但输入的是 ROI 图像，坐标系统不匹配
    // 导致 ReduceDomain 无法正确裁剪，返回整个图像

    std::vector<cv::Point> cvPolygon;
    for (const QPointF& pt : polygon) {
        cvPolygon.push_back(cv::Point(pt.x(), pt.y()));
    }
    cv::Rect boundingRect = cv::boundingRect(cvPolygon);

    // 确保矩形在图像范围内
    boundingRect &= cv::Rect(0, 0, image.cols, image.rows);

    if (boundingRect.width <= 0 || boundingRect.height <= 0) {
        Logger::instance()->error("[Shape] 外接矩形无效");
        return ImageUtils::Mat2HImage(image);
    }

    Logger::instance()->info(QString("[Shape] 外接矩形: x=%1, y=%2, w=%3, h=%4")
                                 .arg(boundingRect.x).arg(boundingRect.y)
                                 .arg(boundingRect.width).arg(boundingRect.height));

    // 提取矩形 ROI
    cv::Mat roi = image(boundingRect).clone();
    HImage result = ImageUtils::Mat2HImage(roi);

    Logger::instance()->info(QString("[Shape] 提取的 ROI 大小: %1x%2").arg(roi.cols).arg(roi.rows));

    return result;
}

void ShapeMatchStrategy::extractTemplateContour(const QVector<QPointF> &polygon)
{
    try {
        m_templateRows.Clear();
        m_templateCols.Clear();

        // 计算模板中心
        double templateCenterRow = m_roiOffsetY + m_templateHeight / 2.0;
        double templateCenterCol = m_roiOffsetX + m_templateWidth / 2.0;

        // 存储相对于模板中心的偏移，而不是绝对坐标
        // 这样在 drawSingleMatch 中就可以直接旋转和平移，无需复杂的坐标转换
        for (const QPointF& pt : polygon) {
            double offsetRow = pt.y() - templateCenterRow;
            double offsetCol = pt.x() - templateCenterCol;
            m_templateRows.Append(offsetRow);
            m_templateCols.Append(offsetCol);
        }

        Logger::instance()->info(QString("[Shape] 提取模板轮廓成功，轮廓点数: %1 (相对于模板中心)").arg(m_templateRows.Length()));

    } catch (const HException& ex) {
        Logger::instance()->warning(
            QString("[Shape] 提取模板轮廓失败: %1").arg(ex.ErrorMessage().Text())
            );
    }
}

void ShapeMatchStrategy::drawSingleMatch(QPainter &painter, const MatchResult &match, const QColor &color) const
{
    try {
        if (m_templateRows.Length() < 3) {
            Logger::instance()->warning(QString("[Shape] drawSingleMatch: 轮廓点不足 (%1)").arg(m_templateRows.Length()));
            return;
        }

        HTuple homMat2D;
        HomMat2dIdentity(&homMat2D);
        HomMat2dRotate(homMat2D, match.angle * 0.0174533, 0, 0, &homMat2D);
        HomMat2dTranslate(homMat2D, match.row, match.column, &homMat2D);

        HTuple transformedRows, transformedCols;
        AffineTransPoint2d(homMat2D,
                           m_templateRows,
                           m_templateCols,
                           &transformedRows,
                           &transformedCols);

        QPolygonF polygon;
        for (int i = 0; i < transformedRows.Length(); ++i)
        {
            polygon<<QPointF(transformedCols[i].D(), transformedRows[i].D());
        }

        if (polygon.size() >= 3)
        {
            QColor fillColor = color;
            fillColor.setAlpha(100);
            painter.setBrush(fillColor);
            painter.setPen(Qt::NoPen);
            painter.drawPolygon(polygon);

            painter.setBrush(Qt::NoBrush);
            painter.setPen(QPen(color, 3));
            painter.drawPolygon(polygon);
        }

    }
    catch (const HException& ex)
    {
        Logger::instance()->warning(
            QString("[Shape] 绘制匹配轮廓失败: %1").arg(ex.ErrorMessage().Text())
            );
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

    // ✅ 1. Mat -> QImage
    cv::Mat displayImage = searchImage.clone();
    if (displayImage.channels() == 1) {
        cv::cvtColor(displayImage, displayImage, cv::COLOR_GRAY2BGR);
    }
    QImage qImage = ImageUtils::Mat2Qimage(displayImage);

    // ✅ 2. 使用 QPainter 绘制
    QPainter painter(&qImage);
    painter.setRenderHint(QPainter::Antialiasing, true);

    // NCC不支持轮廓绘制，使用矩形框
    for (int i = 0; i < matches.size(); ++i) {
        // 根据匹配质量选择颜色
        QColor color;
        if (matches[i].score >= 0.8) {
            color = QColor(0, 255, 0);      // 绿色
        } else if (matches[i].score >= 0.6) {
            color = QColor(255, 255, 0);    // 黄色
        } else {
            color = QColor(255, 165, 0);    // 橙色
        }

        // 计算旋转矩形的四个顶点
        double halfWidth = m_templateWidth / 2.0;
        double halfHeight = m_templateHeight / 2.0;
        double angleRad = matches[i].angle * 0.0174533;
        double cosA = cos(angleRad);
        double sinA = sin(angleRad);

        // 四个角点（未旋转）
        QPointF corners[4] = {
            QPointF(-halfWidth, -halfHeight),
            QPointF(halfWidth, -halfHeight),
            QPointF(halfWidth, halfHeight),
            QPointF(-halfWidth, halfHeight)
        };

        // ✅ 旋转并平移
        QPolygonF rotatedRect;
        for (int j = 0; j < 4; ++j) {
            double x = corners[j].x() * cosA - corners[j].y() * sinA + matches[i].column;
            double y = corners[j].x() * sinA + corners[j].y() * cosA + matches[i].row;
            rotatedRect << QPointF(x, y);
        }

        // ✅ 绘制旋转矩形
        painter.setBrush(Qt::NoBrush);
        painter.setPen(QPen(color, 2));
        painter.drawPolygon(rotatedRect);

        // ✅ 绘制中心点
        QPointF center(matches[i].column, matches[i].row);
        painter.setBrush(color);
        painter.setPen(Qt::NoPen);
        painter.drawEllipse(center, 5, 5);

        painter.setBrush(Qt::NoBrush);
        painter.setPen(QPen(color, 2));
        painter.drawEllipse(center, 8, 8);

        // ✅ 绘制文字信息
        QString info = QString("#%1 Score:%2 Angle:%3°")
                           .arg(i + 1)
                           .arg(matches[i].score, 0, 'f', 2)
                           .arg(matches[i].angle, 0, 'f', 1);

        painter.setPen(color);
        QFont font("Arial", 10, QFont::Bold);
        painter.setFont(font);
        painter.drawText(QPointF(matches[i].column + 15, matches[i].row - 15), info);
    }

    painter.end();

    // ✅ 3. QImage -> Mat
    cv::Mat result = ImageUtils::Qimage2Mat(qImage, true);
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

        for (int i = 0; i < maxMatches && foundCount < maxMatches; ++i)
        {
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
            int maskRadius = (std::max)(templateGray.cols, templateGray.rows) / 2;
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
