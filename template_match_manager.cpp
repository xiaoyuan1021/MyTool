#include "template_match_manager.h"
#include "logger.h"
#include <QDebug>

TemplateMatchManager::TemplateMatchManager(QObject* parent)
    : QObject(parent)
{
    // 设置默认参数
    m_defaultParams.numLevels = 0;        // auto
    m_defaultParams.angleStart = -10.0;   // -10度
    m_defaultParams.angleExtent = 20.0;   // 范围20度
    m_defaultParams.angleStep = 1.0;      // 步长1度
    m_defaultParams.optimization = "auto";
    m_defaultParams.metric = "use_polarity";
    // m_defaultParams.contrast = 150;
    // m_defaultParams.minContrast = 70;
}

TemplateMatchManager::~TemplateMatchManager()
{
    clearAllTemplates();
}

// ========== 创建模板 ==========
bool TemplateMatchManager::createTemplate(const QString& name,
                                          const cv::Mat& fullImage,
                                          const QVector<QPointF>& polygon,
                                          const TemplateData& params)
{
    if (fullImage.empty())
    {
        Logger::instance()->error("创建模板失败：图像为空");
        return false;
    }

    if (polygon.size() < 3)
    {
        Logger::instance()->error("创建模板失败：多边形顶点数不足");
        return false;
    }

    try {
        // 1️⃣ 创建模板区域
        HImage templateRegion = createTemplateRegion(fullImage, polygon);

        // 2️⃣ 创建 Shape Model
        TemplateData newTemplate = params;
        newTemplate.name = name;
        newTemplate.polygonPoints = polygon;

        newTemplate.model.CreateShapeModel(
            templateRegion,
            params.numLevels,
            params.angleStart * 0.0174533,  // 转弧度
            params.angleExtent * 0.0174533,
            params.angleStep * 0.0174533,
            params.optimization.toStdString().c_str(),
            params.metric.toStdString().c_str(),
            "auto",
            "auto"
            );

        // 3️⃣ 保存模板图像（用于显示）
        // 提取多边形区域的 OpenCV 图像
        std::vector<cv::Point> cvPolygon;
        for (const QPointF& pt : polygon)
        {
            cvPolygon.push_back(cv::Point(pt.x(), pt.y()));
        }

        cv::Rect boundingRect = cv::boundingRect(cvPolygon);
        newTemplate.templateImage = fullImage(boundingRect).clone();

        // 4️⃣ 添加到列表
        m_templates.append(newTemplate);

        Logger::instance()->info(
            QString("✅ 模板创建成功: %1 (索引: %2)")
                .arg(name).arg(m_templates.size() - 1)
            );

        emit templateCreated(name);
        return true;

    } catch (const HException& ex) {
        Logger::instance()->error(
            QString("创建模板失败: %1").arg(ex.ErrorMessage().Text())
            );
        return false;
    }
}

// ========== 查找模板 ==========
QVector<MatchResult> TemplateMatchManager::findTemplate(
    const cv::Mat& searchImage,
    int templateIndex,
    double minScore,
    int maxMatches,
    double greediness)
{
    QVector<MatchResult> results;

    // 1️⃣ 参数检查
    if (searchImage.empty()) {
        Logger::instance()->error("匹配失败：搜索图像为空");
        return results;
    }

    if (templateIndex < 0 || templateIndex >= m_templates.size()) {
        Logger::instance()->error("匹配失败：模板索引无效");
        return results;
    }

    try {
        // 2️⃣ 转换搜索图像
        HImage searchHImage = ImageUtils::Mat2HImage(searchImage);

        // 3️⃣ 查找模板
        HTuple row, column, angle, score;

        m_templates[templateIndex].model.FindShapeModel(
            searchHImage,
            0,                    // AngleStart (弧度)
            6.28,                 // AngleExtent (2π)
            minScore,             // MinScore
            maxMatches,           // NumMatches
            0.5,                  // MaxOverlap
            "least_squares",      // SubPixel
            0,                    // NumLevels (0=all)
            greediness,           // Greediness
            &row, &column, &angle, &score
            );

        // 4️⃣ 解析结果
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
            QString("✅ 找到 %1 个匹配 (模板: %2, 最低分数: %3)")
                .arg(numFound)
                .arg(m_templates[templateIndex].name)
                .arg(minScore)
            );

        emit matchCompleted(numFound);

    } catch (const HException& ex) {
        Logger::instance()->error(
            QString("模板匹配失败: %1").arg(ex.ErrorMessage().Text())
            );
    }

    return results;
}

Mat TemplateMatchManager::drawMatches(const Mat &searchImage, int templateIndex, const QVector<MatchResult> &matches) const
{
    // 1️⃣ 参数检查
    if (searchImage.empty() || matches.isEmpty()) {
        return searchImage.clone();
    }

    if (templateIndex < 0 || templateIndex >= m_templates.size()) {
        Logger::instance()->error("绘制失败:模板索引无效");
        return searchImage.clone();
    }

    // 2️⃣ 创建输出图像
    cv::Mat result = searchImage.clone();
    if (result.channels() == 1) {
        cv::cvtColor(result, result, cv::COLOR_GRAY2BGR);
    }

    // 3️⃣ 获取模板数据
    const TemplateData& templateData = m_templates[templateIndex];

    // 4️⃣ 为每个匹配绘制轮廓
    for (int i = 0; i < matches.size(); ++i)
    {
        // 根据匹配质量选择颜色
        cv::Scalar color;
        if (matches[i].score >= 0.8) {
            color = cv::Scalar(0, 255, 0);      // 绿色 - 高质量
        } else if (matches[i].score >= 0.6) {
            color = cv::Scalar(0, 255, 255);    // 黄色 - 中等质量
        } else {
            color = cv::Scalar(0, 165, 255);    // 橙色 - 低质量
        }

        // 绘制单个匹配
        drawSingleMatch(result, templateData, matches[i], color);

        // 绘制中心点
        cv::Point center(matches[i].column, matches[i].row);
        cv::circle(result, center, 5, color, -1);
        cv::circle(result, center, 8, color, 2);

        // 绘制文字信息
        QString info = QString("Score: %1").arg(matches[i].score, 0, 'f', 2);
        cv::putText(result, info.toStdString(),
                    cv::Point(matches[i].column + 15, matches[i].row - 15),
                    cv::FONT_HERSHEY_SIMPLEX, 0.5, color, 2);
    }

    return result;
}

// ========== 模板管理 ==========
QStringList TemplateMatchManager::getTemplateNames() const
{
    QStringList names;
    for (const auto& tmpl : m_templates) {
        names.append(tmpl.name);
    }
    return names;
}

TemplateData TemplateMatchManager::getTemplate(int index) const
{
    if (index >= 0 && index < m_templates.size()) {
        return m_templates[index];
    }
    return TemplateData();
}

cv::Mat TemplateMatchManager::getTemplateImage(int index) const
{
    if (index >= 0 && index < m_templates.size()) {
        return m_templates[index].templateImage;
    }
    return cv::Mat();
}

bool TemplateMatchManager::removeTemplate(int index)
{
    if (index >= 0 && index < m_templates.size()) {
        m_templates.removeAt(index);
        Logger::instance()->info(QString("已删除模板 #%1").arg(index));
        emit templateRemoved(index);
        return true;
    }
    return false;
}

void TemplateMatchManager::clearAllTemplates()
{
    m_templates.clear();
    Logger::instance()->info("已清空所有模板");
}

void TemplateMatchManager::setDefaultParams(const TemplateData& params)
{
    m_defaultParams = params;
}

// ========== 私有辅助函数 ==========
HImage TemplateMatchManager::createTemplateRegion(
    const cv::Mat& image,
    const QVector<QPointF>& polygon)
{
    // 1️⃣ 转换为 HImage
    HImage hImage = ImageUtils::Mat2HImage(image);

    // 2️⃣ 创建多边形 Region
    HTuple rows, cols;
    for (const QPointF& pt : polygon)
    {
        rows.Append(pt.y());
        cols.Append(pt.x());
    }

    HRegion polygonRegion;
    polygonRegion.GenRegionPolygon(rows, cols);

    // 3️⃣ 裁剪图像
    HImage templateImage = hImage.ReduceDomain(polygonRegion);

    return templateImage;
}

void TemplateMatchManager::extractTemplateContour(TemplateData &templateData, const QVector<QPointF> &polygon)
{
    try {
        // 1️⃣ 创建多边形轮廓
        HTuple rows, cols;
        for (const QPointF& pt : polygon)
        {
            rows.Append(pt.y());
            cols.Append(pt.x());
        }

        // 2️⃣ 生成轮廓Region
        templateData.templateContour.GenRegionPolygon(rows, cols);

        // 3️⃣ 获取轮廓边界点(用于精细绘制)
        HRegion boundary = templateData.templateContour.Boundary("inner");
        boundary.GetRegionPoints(&templateData.templateRows, &templateData.templateCols);

        // 如果边界点太少,使用原始多边形点
        if (templateData.templateRows.Length() < 4) {
            templateData.templateRows = rows;
            templateData.templateCols = cols;
        }

    } catch (const HException& ex) {
        Logger::instance()->warning(
            QString("提取模板轮廓失败: %1").arg(ex.ErrorMessage().Text())
            );
        // 使用原始多边形作为后备
        HTuple rows, cols;
        for (const QPointF& pt : polygon) {
            rows.Append(pt.y());
            cols.Append(pt.x());
        }
        templateData.templateRows = rows;
        templateData.templateCols = cols;
    }
}

void TemplateMatchManager::drawSingleMatch(Mat &image, const TemplateData &templateData, const MatchResult &match, const Scalar &color) const
{
    try {
        // 📖 Halcon仿射变换原理讲解:
        //
        // 1. 获取模板的参考中心点
        //    CreateShapeModel 会自动计算模板的中心点作为参考点
        //    我们需要通过 GetShapeModelOrigin 获取这个参考点

        // 2. 构建仿射变换矩阵
        //    变换包括三个步骤:
        //    a) 平移到原点: 将模板中心移到(0,0)
        //    b) 旋转: 按照匹配角度旋转
        //    c) 平移到匹配位置: 移到 (match.column, match.row)

        // 1️⃣ 获取模板中心点
        double modelRow, modelCol;
        templateData.model.GetShapeModelOrigin(&modelRow, &modelCol);

        // 2️⃣ 构建仿射变换矩阵
        // Halcon的仿射变换使用 HomMat2d (2D齐次变换矩阵)
        HTuple homMat2D;

        // 创建单位矩阵
        HomMat2dIdentity(&homMat2D);

        // 平移到原点(反向平移模板中心)
        HomMat2dTranslate(homMat2D, -modelRow, -modelCol, &homMat2D);

        // 旋转(角度已经是弧度)
        double angleRad = match.angle * 0.0174533;  // 度转弧度
        HomMat2dRotate(homMat2D, angleRad, 0, 0, &homMat2D);

        // 平移到匹配位置
        HomMat2dTranslate(homMat2D, match.row, match.column, &homMat2D);

        // 3️⃣ 对轮廓点应用仿射变换
        HTuple transformedRows, transformedCols;
        AffineTransPoint2d(homMat2D,
                           templateData.templateRows,
                           templateData.templateCols,
                           &transformedRows,
                           &transformedCols);

        // 4️⃣ 转换为OpenCV点并绘制
        std::vector<cv::Point> contourPoints;
        for (int i = 0; i < transformedRows.Length(); ++i)
        {
            contourPoints.push_back(
                cv::Point(transformedCols[i].D(), transformedRows[i].D())
                );
        }

        // 绘制填充多边形(半透明)
        if (contourPoints.size() >= 3)
        {
            cv::Mat overlay = image.clone();
            cv::fillPoly(overlay, contourPoints, color);
            cv::addWeighted(overlay, 0.3, image, 0.7, 0, image);

            // 绘制轮廓线
            cv::polylines(image, contourPoints, true, color, 2);
        }

    } catch (const HException& ex) {
        Logger::instance()->warning(
            QString("绘制匹配轮廓失败: %1").arg(ex.ErrorMessage().Text())
            );

        // 降级方案:绘制简单矩形
        double modelRow, modelCol;
        templateData.model.GetShapeModelOrigin(&modelRow, &modelCol);

        cv::Rect rect(
            match.column - 50,
            match.row - 50,
            100,
            100
            );
        cv::rectangle(image, rect, color, 2);
    }
}
