#include "halcon_algorithm.h"
#include "image_processor.h"

HalconAlgorithm::HalconAlgorithm() {}

// ✅ 核心调度方法：根据AlgorithmStep执行对应算法
HRegion HalconAlgorithm::execute(const HRegion& region, const AlgorithmStep& step)
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
        return closeingCircle(region, radius);
    }

    case HalconAlgoType::ClosingRect:
    {
        int width = step.params.value("width", 5).toInt();
        int height = step.params.value("height", 5).toInt();
        return closeingRectangle(region, width, height);
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
        return Union(region);

    case HalconAlgoType::Connection:
        return Connection(region);

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
HRegion HalconAlgorithm::openingCircle(const HRegion &region, double radius)
{
    if(radius < 0.0) return region;
    return region.OpeningCircle(radius);
}

HRegion HalconAlgorithm::openingRectangle(const HRegion &region, Hlong width, Hlong height)
{
    if(width < 0.0 || height < 0.0) return region;
    return region.OpeningRectangle1(width, height);
}

HRegion HalconAlgorithm::closeingCircle(const HRegion &region, double radius)
{
    if(radius < 0.0) return region;
    return region.ClosingCircle(radius);
}

HRegion HalconAlgorithm::closeingRectangle(const HRegion &region, Hlong width, Hlong height)
{
    if(width < 0.0 || height < 0.0) return region;
    return region.ClosingRectangle1(width, height);
}

HRegion HalconAlgorithm::dilateCircle(const HRegion& region, double radius)
{
    if(radius < 0.0) return region;
    return region.DilationCircle(radius);
}

HRegion HalconAlgorithm::dilateRectangle(const HRegion &region, Hlong width, Hlong height)
{
    if(width < 0.0 || height < 0.0) return region;
    return region.DilationRectangle1(width, height);
}

HRegion HalconAlgorithm::erodeCircle(const HRegion& region, double radius)
{
    if(radius < 0.0) return region;
    return region.ErosionCircle(radius);
}

HRegion HalconAlgorithm::erodeRectangle(const HRegion &region, Hlong width, Hlong height)
{
    if(width < 0.0 || height < 0.0) return region;
    return region.ErosionRectangle1(width, height);
}

HRegion HalconAlgorithm::Union(const HRegion &region)
{
    return region.Union1();
}

HRegion HalconAlgorithm::Connection(const HRegion &region)
{
    return region.Connection();
}

HRegion HalconAlgorithm::fillUpHoles(const HRegion &region)
{
    return region.FillUp();
}

HRegion HalconAlgorithm::shapeTrans(const HRegion &region, const HString & type)
{
    return region.ShapeTrans(type);
}

HRegion HalconAlgorithm::selectShapeArea(const HRegion &region, double minArea, double maxArea)
{
    if(minArea < 0.0 || maxArea < minArea) return region;

    try
    {
        HRegion connectedRegions = region.Connection();
        return connectedRegions.SelectShape("area", "and", minArea, maxArea);
    }
    catch (const HException& ex)
    {
        qDebug() << "SelectShape error:" << ex.ErrorMessage().Text();
        return region;
    }
}
