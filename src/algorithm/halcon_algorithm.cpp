#include "halcon_algorithm.h"
#include "image_processor.h"
#include "image_utils.h"
#include "logger.h"
#include <QPolygonF>


HalconAlgorithm::HalconAlgorithm() {}

// ✅ 核心调度方法：根据AlgorithmStep执行对应算法
HalconCpp::HRegion HalconAlgorithm::execute(const HalconCpp::HRegion& region, const AlgorithmStep& step)
{
    HalconAlgoType algoType = static_cast<HalconAlgoType>(
        step.params["HalconAlgoType"].toInt()
        );

    switch(algoType)
    {
    case HalconAlgoType::OpeningCircle:
    {
        double radius = step.params.value("radius", 3.5).toDouble();
        return openingCircle(region, radius);
    }

    case HalconAlgoType::OpeningRect:
    {
        int width = step.params.value("width", 5).toInt();
        int height = step.params.value("height", 5).toInt();
        return openingRectangle(region, width, height);
    }

    case HalconAlgoType::ClosingCircle:
    {
        double radius = step.params.value("radius", 3.5).toDouble();
        return closingCircle(region, radius);
    }

    case HalconAlgoType::ClosingRect:
    {
        int width = step.params.value("width", 5).toInt();
        int height = step.params.value("height", 5).toInt();
        return closingRectangle(region, width, height);
    }

    case HalconAlgoType::DilationCircle:
    {
        double radius = step.params.value("radius", 3.5).toDouble();
        return dilateCircle(region, radius);
    }

    case HalconAlgoType::DilationRect:
    {
        int width = step.params.value("width", 5).toInt();
        int height = step.params.value("height", 5).toInt();
        return dilateRectangle(region, width, height);
    }

    case HalconAlgoType::ErosionCircle:
    {
        double radius = step.params.value("radius", 3.5).toDouble();
        return erodeCircle(region, radius);
    }

    case HalconAlgoType::ErosionRect:
    {
        int width = step.params.value("width", 5).toInt();
        int height = step.params.value("height", 5).toInt();
        return erodeRectangle(region, width, height);
    }

    case HalconAlgoType::Union:
        return union1(region);

    case HalconAlgoType::Connection:
        return connection(region);

    case HalconAlgoType::FillUp:
        return fillUpHoles(region);

    case HalconAlgoType::ShapeTrans:
    {
        QString transType = step.params.value("shapeType", "convex").toString();
        return shapeTrans(region, transType.toStdString().c_str());
    }

    default:
        return region; // 未知类型，保持不变
    }
}

// =============== 以下为private方法实现 ===============
HalconCpp::HRegion HalconAlgorithm::openingCircle(const HalconCpp::HRegion &region, double radius)
{
    if(radius < 0.0) return region;
    return region.OpeningCircle(radius);
}

HalconCpp::HRegion HalconAlgorithm::openingRectangle(const HalconCpp::HRegion &region, Hlong width, Hlong height)
{
    if(width < 0.0 || height < 0.0) return region;
    return region.OpeningRectangle1(width, height);
}

HalconCpp::HRegion HalconAlgorithm::closingCircle(const HalconCpp::HRegion &region, double radius)
{
    if(radius < 0.0) return region;
    return region.ClosingCircle(radius);
}

HalconCpp::HRegion HalconAlgorithm::closingRectangle(const HalconCpp::HRegion &region, Hlong width, Hlong height)
{
    if(width < 0.0 || height < 0.0) return region;
    return region.ClosingRectangle1(width, height);
}

HalconCpp::HRegion HalconAlgorithm::dilateCircle(const HalconCpp::HRegion& region, double radius)
{
    if(radius < 0.0) return region;
    return region.DilationCircle(radius);
}

HalconCpp::HRegion HalconAlgorithm::dilateRectangle(const HalconCpp::HRegion &region, Hlong width, Hlong height)
{
    if(width < 0.0 || height < 0.0) return region;
    return region.DilationRectangle1(width, height);
}

HalconCpp::HRegion HalconAlgorithm::erodeCircle(const HalconCpp::HRegion& region, double radius)
{
    if(radius < 0.0) return region;
    return region.ErosionCircle(radius);
}

HalconCpp::HRegion HalconAlgorithm::erodeRectangle(const HalconCpp::HRegion &region, Hlong width, Hlong height)
{
    if(width < 0.0 || height < 0.0) return region;
    return region.ErosionRectangle1(width, height);
}

HalconCpp::HRegion HalconAlgorithm::union1(const HalconCpp::HRegion &region)
{
    return region.Union1();
}

HalconCpp::HRegion HalconAlgorithm::connection(const HalconCpp::HRegion &region)
{
    return region.Connection();
}

HalconCpp::HRegion HalconAlgorithm::fillUpHoles(const HalconCpp::HRegion &region)
{
    return region.FillUp();
}

HalconCpp::HRegion HalconAlgorithm::shapeTrans(const HalconCpp::HRegion &region, const HalconCpp::HString & type)
{
    return region.ShapeTrans(type);
}

HalconCpp::HRegion HalconAlgorithm::selectShapeArea(const HalconCpp::HRegion &region, double minArea, double maxArea)
{
    if(minArea < 0.0 || maxArea < minArea) return region;

    try
    {
        HalconCpp::HRegion connectedRegions = region.Connection();
        return connectedRegions.SelectShape("area", "and", minArea, maxArea);
    }
    catch (const HException& ex)
    {
        Logger::instance()->error(QString("SelectShape error: %1").arg(ex.ErrorMessage().Text()));
        return region;
    }
}


QVector<RegionFeature> HalconAlgorithm::analyzeRegionsInPolygon(
    const QVector<QPointF>& polygon,
    const cv::Mat& processedImage)
{
    QVector<RegionFeature> results;

    // ========== 1. 参数检查 ==========
    if (polygon.size() < 3) {
        Logger::instance()->warning("顶点数量不足,至少需要3个点");
        return results;
    }

    if (processedImage.empty()) {
        Logger::instance()->error("处理后的图像为空");
        return results;
    }

    try {
        // ========== 2. 创建 Halcon 多边形区域 ==========
        HalconCpp::HTuple rows, cols;
        for (const QPointF& pt : polygon) {
            rows.Append(pt.y());
            cols.Append(pt.x());
        }

        // 创建多边形区域
        HalconCpp::HRegion polygonRegion;
        polygonRegion.GenRegionPolygon(rows, cols);

        // ========== 3. 转成 HRegion 并做连通域分析 ==========
        HalconCpp::HRegion allRegions = ImageUtils::MatToHRegion(processedImage);
        HalconCpp::HRegion connectedRegions = allRegions.Connection();

        // ========== 4. 使用批量操作：先与多边形求交集 ==========
        // 关键优化：先对所有连通域与多边形求交集，然后批量筛选有交集的区域
        HalconCpp::HRegion intersectedRegions = connectedRegions.Intersection(polygonRegion);
        
        // 使用 SelectShape 批量筛选面积大于0的区域（即与多边形有交集的区域）
        HalconCpp::HRegion validRegions = intersectedRegions.SelectShape("area", "and", 0.01, 99999999.0);
        
        // 统计有效区域数量
        HalconCpp::HTuple validNum;
        CountObj(validRegions, &validNum);
        int validCount = validNum[0].I();

        if (validCount == 0) {
            Logger::instance()->warning("ROI区域内没有找到目标");
            return results;
        }

        // ========== 5. 批量计算特征 ==========
        // 使用 AreaCenter 批量计算面积和中心点
        HalconCpp::HTuple areas, centerRows, centerCols;
        areas = validRegions.AreaCenter(&centerRows, &centerCols);
        
        // 使用 Circularity 批量计算圆度
        HalconCpp::HTuple circularities;
        circularities = validRegions.Circularity();
        
        // 使用 SmallestRectangle1 批量计算外接矩形
        HalconCpp::HTuple row1s, col1s, row2s, col2s;
        validRegions.SmallestRectangle1(&row1s, &col1s, &row2s, &col2s);

        // ========== 6. 构建结果 ==========
        for (int i = 0; i < validCount; ++i) {
            RegionFeature feature;
            feature.index = i + 1;
            feature.area = areas[i].D();
            feature.centerX = centerCols[i].D();
            feature.centerY = centerRows[i].D();
            feature.circularity = circularities[i].D();
            feature.width = col2s[i].D() - col1s[i].D();
            feature.height = row2s[i].D() - row1s[i].D();
            results.append(feature);
        }

        Logger::instance()->info(QString("找到 %1 个目标").arg(validCount));

    }
    catch (const HException& ex)
    {
        Logger::instance()->error(
            QString("Halcon计算错误: %1").arg(ex.ErrorMessage().Text())
            );
    }
    return results;
}
