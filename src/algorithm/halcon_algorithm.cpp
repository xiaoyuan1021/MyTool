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

        // ========== 4. 统计连通域数量 ==========
        HalconCpp::HTuple totalNum;
        CountObj(connectedRegions, &totalNum);
        int totalCount = totalNum[0].I();

        if (totalCount == 0) {
            Logger::instance()->warning("图像中没有找到任何目标");
            return results;
        }

        // ========== 5. 遍历每个连通域,筛选与多边形有交集的 ==========
        for (int i = 1; i <= totalCount; ++i) {
            HalconCpp::HRegion singleRegion;
            HalconCpp::SelectObj(connectedRegions, &singleRegion, i);

            // ✅ 关键修改:计算与多边形的交集
            HalconCpp::HRegion intersection = singleRegion.Intersection(polygonRegion);

            // 检查交集面积
            HalconCpp::HTuple intersectionArea;
            intersectionArea = intersection.Area();

            // 如果交集面积为0,说明不在ROI内
            if (intersectionArea.Length() == 0 || intersectionArea[0].D() <= 0.0) {
                continue;
            }

            // ========== 6. 计算特征 ==========
            RegionFeature feature;
            feature.index = i;

            // 计算面积和中心点
            HalconCpp::HTuple area, centerRow, centerCol;
            area = singleRegion.AreaCenter(&centerRow, &centerCol);
            feature.area = area[0].D();
            feature.centerX = centerCol[0].D();
            feature.centerY = centerRow[0].D();

            // 计算圆度
            HalconCpp::HTuple circularity;
            circularity = singleRegion.Circularity();
            feature.circularity = circularity[0].D();

            // 计算外接矩形(宽度、高度)
            HalconCpp::HTuple row1, col1, row2, col2;
            singleRegion.SmallestRectangle1(&row1, &col1, &row2, &col2);
            feature.width = col2[0].D() - col1[0].D();
            feature.height = row2[0].D() - row1[0].D();

            // 添加到结果列表
            results.append(feature);
        }

        if (results.isEmpty()) {
            Logger::instance()->warning("ROI区域内没有找到目标");
        }

    }
    catch (const HException& ex)
    {
        Logger::instance()->error(
            QString("Halcon计算错误: %1").arg(ex.ErrorMessage().Text())
            );
    }
    return results;
}
