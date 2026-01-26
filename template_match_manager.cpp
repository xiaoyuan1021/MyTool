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
            params.contrast,
            params.minContrast
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
    for (const QPointF& pt : polygon) {
        rows.Append(pt.y());
        cols.Append(pt.x());
    }

    HRegion polygonRegion;
    polygonRegion.GenRegionPolygon(rows, cols);

    // 3️⃣ 裁剪图像
    HImage templateImage = hImage.ReduceDomain(polygonRegion);

    return templateImage;
}
