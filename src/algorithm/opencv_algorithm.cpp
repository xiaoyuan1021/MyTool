#include "opencv_algorithm.h"
#include "image_processor.h"
#include "logger.h"

OpenCVAlgorithm::OpenCVAlgorithm() {}

// =============== 核心调度方法 ===============

cv::Mat OpenCVAlgorithm::execute(const cv::Mat& region, const AlgorithmStep& step)
{
    OpenCVAlgoType algoType = static_cast<OpenCVAlgoType>(
        step.params["OpenCVAlgoType"].toInt()
    );

    switch(algoType)
    {
    case OpenCVAlgoType::OpeningCircle:
    {
        double radius = step.params.value("radius", 3.5).toDouble();
        return openingCircle(region, radius);
    }

    case OpenCVAlgoType::OpeningRect:
    {
        int width = step.params.value("width", 5).toInt();
        int height = step.params.value("height", 5).toInt();
        return openingRectangle(region, width, height);
    }

    case OpenCVAlgoType::ClosingCircle:
    {
        double radius = step.params.value("radius", 3.5).toDouble();
        return closingCircle(region, radius);
    }

    case OpenCVAlgoType::ClosingRect:
    {
        int width = step.params.value("width", 5).toInt();
        int height = step.params.value("height", 5).toInt();
        return closingRectangle(region, width, height);
    }

    case OpenCVAlgoType::DilationCircle:
    {
        double radius = step.params.value("radius", 3.5).toDouble();
        return dilateCircle(region, radius);
    }

    case OpenCVAlgoType::DilationRect:
    {
        int width = step.params.value("width", 5).toInt();
        int height = step.params.value("height", 5).toInt();
        return dilateRectangle(region, width, height);
    }

    case OpenCVAlgoType::ErosionCircle:
    {
        double radius = step.params.value("radius", 3.5).toDouble();
        return erodeCircle(region, radius);
    }

    case OpenCVAlgoType::ErosionRect:
    {
        int width = step.params.value("width", 5).toInt();
        int height = step.params.value("height", 5).toInt();
        return erodeRectangle(region, width, height);
    }

    case OpenCVAlgoType::Union:
        return union1(region);

    case OpenCVAlgoType::Connection:
        return connection(region);

    case OpenCVAlgoType::FillUp:
        return fillUpHoles(region);

    case OpenCVAlgoType::ShapeTrans:
    {
        QString transType = step.params.value("shapeType", "convex").toString();
        
        // 将字符串类型转换为ShapeTransType
        ShapeTransType shapeType = ShapeTransType::Convex;
        if (transType == "convex" || transType == "凸包") {
            shapeType = ShapeTransType::Convex;
        } else if (transType == "rectangle1" || transType == "最小外接矩形") {
            shapeType = ShapeTransType::Rectangle1;
        } else if (transType == "rectangle2" || transType == "旋转外接矩形") {
            shapeType = ShapeTransType::Rectangle2;
        } else if (transType == "inner_circle" || transType == "最大内接圆") {
            shapeType = ShapeTransType::InnerCircle;
        } else if (transType == "outer_circle" || transType == "最小外接圆") {
            shapeType = ShapeTransType::OuterCircle;
        } else if (transType == "ellipse" || transType == "椭圆") {
            shapeType = ShapeTransType::Ellipse;
        }
        
        return shapeTrans(region, shapeType);
    }

    case OpenCVAlgoType::SelectShapeArea:
    {
        double minArea = step.params.value("minArea", 0.0).toDouble();
        double maxArea = step.params.value("maxArea", 99999999.0).toDouble();
        return selectShapeArea(region, minArea, maxArea);
    }

    default:
        Logger::instance()->warning(QString("未知的算法类型: %1").arg(static_cast<int>(algoType)));
        return region.clone(); // 未知类型，保持不变
    }
}

// =============== 辅助函数 ===============

cv::Mat OpenCVAlgorithm::createCircleKernel(int radius)
{
    int ksize = 2 * radius + 1;
    return cv::getStructuringElement(cv::MORPH_ELLIPSE, cv::Size(ksize, ksize));
}

double OpenCVAlgorithm::calculateCircularity(const std::vector<cv::Point>& contour)
{
    if (contour.size() < 3) return 0.0;
    
    double area = cv::contourArea(contour);
    double perimeter = cv::arcLength(contour, true);
    
    if (perimeter <= 0) return 0.0;
    
    // 圆形度公式：4π * 面积 / 周长²
    return 4.0 * CV_PI * area / (perimeter * perimeter);
}

// =============== 形态学操作 ===============

cv::Mat OpenCVAlgorithm::openingCircle(const cv::Mat& region, double radius)
{
    if (radius < 0 || region.empty()) return region.clone();
    
    int kernelRadius = static_cast<int>(std::round(radius));
    cv::Mat kernel = createCircleKernel(kernelRadius);
    cv::Mat result;
    cv::morphologyEx(region, result, cv::MORPH_OPEN, kernel);
    return result;
}

cv::Mat OpenCVAlgorithm::openingRectangle(const cv::Mat& region, int width, int height)
{
    if (width < 0 || height < 0 || region.empty()) return region.clone();
    
    cv::Mat kernel = cv::getStructuringElement(cv::MORPH_RECT, cv::Size(width, height));
    cv::Mat result;
    cv::morphologyEx(region, result, cv::MORPH_OPEN, kernel);
    return result;
}

cv::Mat OpenCVAlgorithm::closingCircle(const cv::Mat& region, double radius)
{
    if (radius < 0 || region.empty()) return region.clone();
    
    int kernelRadius = static_cast<int>(std::round(radius));
    cv::Mat kernel = createCircleKernel(kernelRadius);
    cv::Mat result;
    cv::morphologyEx(region, result, cv::MORPH_CLOSE, kernel);
    return result;
}

cv::Mat OpenCVAlgorithm::closingRectangle(const cv::Mat& region, int width, int height)
{
    if (width < 0 || height < 0 || region.empty()) return region.clone();
    
    cv::Mat kernel = cv::getStructuringElement(cv::MORPH_RECT, cv::Size(width, height));
    cv::Mat result;
    cv::morphologyEx(region, result, cv::MORPH_CLOSE, kernel);
    return result;
}

cv::Mat OpenCVAlgorithm::dilateCircle(const cv::Mat& region, double radius)
{
    if (radius < 0 || region.empty()) return region.clone();
    
    int kernelRadius = static_cast<int>(std::round(radius));
    cv::Mat kernel = createCircleKernel(kernelRadius);
    cv::Mat result;
    cv::dilate(region, result, kernel);
    return result;
}

cv::Mat OpenCVAlgorithm::dilateRectangle(const cv::Mat& region, int width, int height)
{
    if (width < 0 || height < 0 || region.empty()) return region.clone();
    
    cv::Mat kernel = cv::getStructuringElement(cv::MORPH_RECT, cv::Size(width, height));
    cv::Mat result;
    cv::dilate(region, result, kernel);
    return result;
}

cv::Mat OpenCVAlgorithm::erodeCircle(const cv::Mat& region, double radius)
{
    if (radius < 0 || region.empty()) return region.clone();
    
    int kernelRadius = static_cast<int>(std::round(radius));
    cv::Mat kernel = createCircleKernel(kernelRadius);
    cv::Mat result;
    cv::erode(region, result, kernel);
    return result;
}

cv::Mat OpenCVAlgorithm::erodeRectangle(const cv::Mat& region, int width, int height)
{
    if (width < 0 || height < 0 || region.empty()) return region.clone();
    
    cv::Mat kernel = cv::getStructuringElement(cv::MORPH_RECT, cv::Size(width, height));
    cv::Mat result;
    cv::erode(region, result, kernel);
    return result;
}

// =============== 连通域操作 ===============

cv::Mat OpenCVAlgorithm::connection(const cv::Mat& region)
{
    if (region.empty()) return region.clone();
    
    cv::Mat binary;
    if (region.channels() == 3) {
        cv::cvtColor(region, binary, cv::COLOR_BGR2GRAY);
    } else {
        binary = region.clone();
    }
    
    // 确保是二值图像
    if (binary.type() != CV_8UC1) {
        binary.convertTo(binary, CV_8UC1);
    }
    cv::threshold(binary, binary, 127, 255, cv::THRESH_BINARY);
    
    cv::Mat labels;
    cv::connectedComponents(binary, labels, 8);
    
    // ✅ 正确：生成二值图像，每个连通域为255，背景为0
    cv::Mat result = cv::Mat::zeros(binary.size(), CV_8UC1);
    
    // 跳过背景标签0，从1开始
    for (int i = 1; i < cv::connectedComponents(binary, labels, 8); ++i) {
        result.setTo(cv::Scalar(255), labels == i);
    }
    
    return result;
}

cv::Mat OpenCVAlgorithm::union1(const cv::Mat& region)
{
    if (region.empty()) return region.clone();
    
    // 将所有非零区域合并为一个整体
    cv::Mat binary;
    if (region.channels() == 3) {
        cv::cvtColor(region, binary, cv::COLOR_BGR2GRAY);
    } else {
        binary = region.clone();
    }
    
    cv::Mat result;
    cv::threshold(binary, result, 0, 255, cv::THRESH_BINARY);
    return result;
}

// =============== 填充操作 ===============

cv::Mat OpenCVAlgorithm::fillUpHoles(const cv::Mat& region)
{
    if (region.empty()) return region.clone();
    
    cv::Mat binary;
    if (region.channels() == 3) {
        cv::cvtColor(region, binary, cv::COLOR_BGR2GRAY);
    } else {
        binary = region.clone();
    }
    
    // 确保是二值图像
    cv::threshold(binary, binary, 127, 255, cv::THRESH_BINARY);
    
    // 使用floodFill填充孔洞
    cv::Mat filled = binary.clone();
    cv::floodFill(filled, cv::Point(0, 0), cv::Scalar(255));
    
    // 反转填充后的图像
    cv::Mat inverted;
    cv::bitwise_not(filled, inverted);
    
    // 原图和反转图的并集就是填充孔洞后的结果
    cv::Mat result;
    cv::bitwise_or(binary, inverted, result);
    
    return result;
}

// =============== 形状变换 ===============

cv::Mat OpenCVAlgorithm::shapeTrans(const cv::Mat& region, ShapeTransType type)
{
    if (region.empty()) return region.clone();
    
    cv::Mat binary;
    if (region.channels() == 3) {
        cv::cvtColor(region, binary, cv::COLOR_BGR2GRAY);
    } else {
        binary = region.clone();
    }
    
    // 确保是二值图像
    cv::threshold(binary, binary, 127, 255, cv::THRESH_BINARY);
    
    std::vector<std::vector<cv::Point>> contours;
    cv::findContours(binary, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);
    
    if (contours.empty()) return region.clone();
    
    cv::Mat result = cv::Mat::zeros(binary.size(), CV_8UC1);
    
    // 合并所有轮廓
    std::vector<cv::Point> allPoints;
    for (const auto& contour : contours) {
        allPoints.insert(allPoints.end(), contour.begin(), contour.end());
    }
    
    switch (type) {
        case ShapeTransType::Convex: {
            std::vector<cv::Point> hull;
            cv::convexHull(allPoints, hull);
            std::vector<std::vector<cv::Point>> hullContours = {hull};
            cv::drawContours(result, hullContours, 0, cv::Scalar(255), cv::FILLED);
            break;
        }
        case ShapeTransType::Rectangle1: {
            cv::Rect rect = cv::boundingRect(allPoints);
            cv::rectangle(result, rect, cv::Scalar(255), cv::FILLED);
            break;
        }
        case ShapeTransType::Rectangle2: {
            cv::RotatedRect rotRect = cv::minAreaRect(allPoints);
            cv::Point2f vertices[4];
            rotRect.points(vertices);
            std::vector<cv::Point> pts;
            for (int i = 0; i < 4; ++i) {
                pts.push_back(cv::Point(vertices[i].x, vertices[i].y));
            }
            std::vector<std::vector<cv::Point>> polyContours = {pts};
            cv::drawContours(result, polyContours, 0, cv::Scalar(255), cv::FILLED);
            break;
        }
        case ShapeTransType::OuterCircle: {
            cv::Point2f center;
            float radius;
            cv::minEnclosingCircle(allPoints, center, radius);
            cv::circle(result, center, static_cast<int>(radius), cv::Scalar(255), cv::FILLED);
            break;
        }
        case ShapeTransType::Ellipse: {
            if (allPoints.size() >= 5) {
                cv::RotatedRect ellipse = cv::fitEllipse(allPoints);
                cv::ellipse(result, ellipse, cv::Scalar(255), cv::FILLED);
            }
            break;
        }
        case ShapeTransType::InnerCircle: {
            // 最大内接圆：使用距离变换
            cv::Mat dist;
            cv::distanceTransform(binary, dist, cv::DIST_L2, cv::DIST_MASK_5);
            double maxVal;
            cv::Point maxLoc;
            cv::minMaxLoc(dist, nullptr, &maxVal, nullptr, &maxLoc);
            cv::circle(result, maxLoc, static_cast<int>(maxVal), cv::Scalar(255), cv::FILLED);
            break;
        }
    }
    
    return result;
}

// =============== 区域筛选 ===============

cv::Mat OpenCVAlgorithm::selectShapeArea(const cv::Mat& region, double minArea, double maxArea)
{
    if (minArea < 0 || maxArea < minArea || region.empty()) return region.clone();
    
    cv::Mat binary;
    if (region.channels() == 3) {
        cv::cvtColor(region, binary, cv::COLOR_BGR2GRAY);
    } else {
        binary = region.clone();
    }
    
    cv::threshold(binary, binary, 127, 255, cv::THRESH_BINARY);
    
    std::vector<std::vector<cv::Point>> contours;
    cv::findContours(binary, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);
    
    cv::Mat result = cv::Mat::zeros(binary.size(), CV_8UC1);
    
    for (const auto& contour : contours) {
        double area = cv::contourArea(contour);
        if (area >= minArea && area <= maxArea) {
            cv::drawContours(result, std::vector<std::vector<cv::Point>>{contour}, 
                           0, cv::Scalar(255), cv::FILLED);
        }
    }
    
    return result;
}

// =============== 区域分析 ===============

QVector<OpenCVAlgorithm::RegionFeature> OpenCVAlgorithm::analyzeRegionsInPolygon(
    const QVector<QPointF>& polygon,
    const cv::Mat& processedImage)
{
    QVector<RegionFeature> results;
    
    if (polygon.size() < 3) {
        Logger::instance()->warning("顶点数量不足，至少需要3个点");
        return results;
    }
    
    if (processedImage.empty()) {
        Logger::instance()->error("处理后的图像为空");
        return results;
    }
    
    try {
        // 1. 创建多边形掩码
        std::vector<cv::Point> cvPolygon;
        for (const QPointF& pt : polygon) {
            cvPolygon.push_back(cv::Point(static_cast<int>(pt.x()), static_cast<int>(pt.y())));
        }
        
        cv::Mat polygonMask = cv::Mat::zeros(processedImage.size(), CV_8UC1);
        cv::fillPoly(polygonMask, std::vector<std::vector<cv::Point>>{cvPolygon}, cv::Scalar(255));
        
        // 2. 准备二值图像
        cv::Mat binary;
        if (processedImage.channels() == 3) {
            cv::cvtColor(processedImage, binary, cv::COLOR_BGR2GRAY);
        } else {
            binary = processedImage.clone();
        }
        cv::threshold(binary, binary, 127, 255, cv::THRESH_BINARY);
        
        // 3. 与多边形求交集
        cv::Mat masked;
        cv::bitwise_and(binary, polygonMask, masked);
        
        // 4. 连通域分析
        cv::Mat labels, stats, centroids;
        int numLabels = cv::connectedComponentsWithStats(masked, labels, stats, centroids, 8);
        
        if (numLabels <= 1) {
            Logger::instance()->warning("ROI区域内没有找到目标");
            return results;
        }
        
        // 5. 遍历连通域（跳过背景标签0）
        for (int i = 1; i < numLabels; ++i) {
            double area = stats.at<int>(i, cv::CC_STAT_AREA);
            if (area < 0.01) continue; // 跳过太小的区域
            
            double cx = centroids.at<double>(i, 0);
            double cy = centroids.at<double>(i, 1);
            int x = stats.at<int>(i, cv::CC_STAT_LEFT);
            int y = stats.at<int>(i, cv::CC_STAT_TOP);
            int w = stats.at<int>(i, cv::CC_STAT_WIDTH);
            int h = stats.at<int>(i, cv::CC_STAT_HEIGHT);
            
            // 计算圆形度
            cv::Mat regionMask = (labels == i);
            std::vector<std::vector<cv::Point>> regionContours;
            cv::findContours(regionMask, regionContours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);
            
            double circularity = 0.0;
            if (!regionContours.empty()) {
                circularity = calculateCircularity(regionContours[0]);
            }
            
            RegionFeature feature;
            feature.index = i;
            feature.area = area;
            feature.centerX = cx;
            feature.centerY = cy;
            feature.circularity = circularity;
            feature.width = w;
            feature.height = h;
            
            results.append(feature);
        }
        
        Logger::instance()->info(QString("找到 %1 个目标").arg(results.size()));
        
    } catch (const cv::Exception& ex) {
        Logger::instance()->error(QString("OpenCV计算错误: %1").arg(ex.what()));
    }
    
    return results;
}

// =============== 区域特征计算 ===============

double OpenCVAlgorithm::calculateArea(const cv::Mat& region)
{
    if (region.empty()) return 0.0;
    
    cv::Mat binary;
    if (region.channels() == 3) {
        cv::cvtColor(region, binary, cv::COLOR_BGR2GRAY);
    } else {
        binary = region.clone();
    }
    
    cv::threshold(binary, binary, 127, 255, cv::THRESH_BINARY);
    
    // 统计非零像素数量作为面积
    return static_cast<double>(cv::countNonZero(binary));
}

double OpenCVAlgorithm::calculateRegionCircularity(const cv::Mat& region)
{
    if (region.empty()) return 0.0;
    
    cv::Mat binary;
    if (region.channels() == 3) {
        cv::cvtColor(region, binary, cv::COLOR_BGR2GRAY);
    } else {
        binary = region.clone();
    }
    
    cv::threshold(binary, binary, 127, 255, cv::THRESH_BINARY);
    
    std::vector<std::vector<cv::Point>> contours;
    cv::findContours(binary, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);
    
    if (contours.empty()) return 0.0;
    
    // 如果有多个轮廓，合并后计算
    if (contours.size() == 1) {
        return calculateCircularity(contours[0]);
    }
    
    // 合并所有轮廓
    std::vector<cv::Point> allPoints;
    for (const auto& contour : contours) {
        allPoints.insert(allPoints.end(), contour.begin(), contour.end());
    }
    
    return calculateCircularity(allPoints);
}

double OpenCVAlgorithm::calculatePerimeter(const cv::Mat& region)
{
    if (region.empty()) return 0.0;
    
    cv::Mat binary;
    if (region.channels() == 3) {
        cv::cvtColor(region, binary, cv::COLOR_BGR2GRAY);
    } else {
        binary = region.clone();
    }
    
    cv::threshold(binary, binary, 127, 255, cv::THRESH_BINARY);
    
    std::vector<std::vector<cv::Point>> contours;
    cv::findContours(binary, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);
    
    if (contours.empty()) return 0.0;
    
    double totalPerimeter = 0.0;
    for (const auto& contour : contours) {
        totalPerimeter += cv::arcLength(contour, true);
    }
    
    return totalPerimeter;
}

OpenCVAlgorithm::RegionFeature OpenCVAlgorithm::calculateRegionFeature(const cv::Mat& region, int index)
{
    RegionFeature feature;
    feature.index = index;
    feature.area = 0.0;
    feature.centerX = 0.0;
    feature.centerY = 0.0;
    feature.circularity = 0.0;
    feature.width = 0.0;
    feature.height = 0.0;
    
    if (region.empty()) return feature;
    
    cv::Mat binary;
    if (region.channels() == 3) {
        cv::cvtColor(region, binary, cv::COLOR_BGR2GRAY);
    } else {
        binary = region.clone();
    }
    
    cv::threshold(binary, binary, 127, 255, cv::THRESH_BINARY);
    
    // 计算面积
    feature.area = cv::countNonZero(binary);
    
    // 计算中心点（使用矩）
    cv::Moments m = cv::moments(binary, true);
    if (m.m00 > 0) {
        feature.centerX = m.m10 / m.m00;
        feature.centerY = m.m01 / m.m00;
    }
    
    // 计算外接矩形
    cv::Rect bbox = cv::boundingRect(binary);
    feature.width = bbox.width;
    feature.height = bbox.height;
    
    // 计算圆度
    std::vector<std::vector<cv::Point>> contours;
    cv::findContours(binary, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);
    
    if (!contours.empty()) {
        // 合并所有轮廓
        std::vector<cv::Point> allPoints;
        for (const auto& contour : contours) {
            allPoints.insert(allPoints.end(), contour.begin(), contour.end());
        }
        feature.circularity = calculateCircularity(allPoints);
    }
    
    return feature;
}

// =============== 新增特征计算函数 ===============

double OpenCVAlgorithm::calculateCompactness(const std::vector<cv::Point>& contour)
{
    if (contour.size() < 3) return 0.0;
    
    double area = cv::contourArea(contour);
    double perimeter = cv::arcLength(contour, true);
    
    if (perimeter <= 0) return 0.0;
    
    // 紧凑度公式：4π * 面积 / 周长²（与圆形度相同）
    return 4.0 * CV_PI * area / (perimeter * perimeter);
}

double OpenCVAlgorithm::calculateConvexity(const std::vector<cv::Point>& contour)
{
    if (contour.size() < 3) return 0.0;
    
    double area = cv::contourArea(contour);
    if (area <= 0) return 0.0;
    
    // 计算凸包
    std::vector<cv::Point> hull;
    cv::convexHull(contour, hull);
    
    double hullArea = cv::contourArea(hull);
    if (hullArea <= 0) return 0.0;
    
    // 凸性 = 面积 / 凸包面积
    return area / hullArea;
}

double OpenCVAlgorithm::calculateRectangularity(const std::vector<cv::Point>& contour)
{
    if (contour.size() < 3) return 0.0;
    
    double area = cv::contourArea(contour);
    if (area <= 0) return 0.0;
    
    // 计算最小外接矩形
    cv::RotatedRect rotRect = cv::minAreaRect(contour);
    double rectArea = rotRect.size.width * rotRect.size.height;
    
    if (rectArea <= 0) return 0.0;
    
    // 矩形度 = 面积 / 最小外接矩形面积
    return area / rectArea;
}

cv::Mat OpenCVAlgorithm::selectShapeByFeature(const cv::Mat& region, const QString& featureName,
                                               double minValue, double maxValue)
{
    if (region.empty()) return region.clone();
    
    cv::Mat binary;
    if (region.channels() == 3) {
        cv::cvtColor(region, binary, cv::COLOR_BGR2GRAY);
    } else {
        binary = region.clone();
    }
    
    cv::threshold(binary, binary, 127, 255, cv::THRESH_BINARY);
    
    // 连通域分析
    cv::Mat labels, stats, centroids;
    int numLabels = cv::connectedComponentsWithStats(binary, labels, stats, centroids, 8);
    
    cv::Mat result = cv::Mat::zeros(binary.size(), CV_8UC1);
    
    for (int i = 1; i < numLabels; ++i) {
        // 提取当前连通域
        cv::Mat regionMask = (labels == i);
        
        // 获取轮廓
        std::vector<std::vector<cv::Point>> contours;
        cv::findContours(regionMask, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);
        
        if (contours.empty()) continue;
        
        const auto& contour = contours[0];
        double featureValue = 0.0;
        
        // 根据特征类型计算值
        if (featureName == "area") {
            featureValue = cv::contourArea(contour);
        } else if (featureName == "circularity") {
            featureValue = calculateCircularity(contour);
        } else if (featureName == "compactness") {
            featureValue = calculateCompactness(contour);
        } else if (featureName == "convexity") {
            featureValue = calculateConvexity(contour);
        } else if (featureName == "width") {
            cv::Rect bbox = cv::boundingRect(contour);
            featureValue = bbox.width;
        } else if (featureName == "height") {
            cv::Rect bbox = cv::boundingRect(contour);
            featureValue = bbox.height;
        } else if (featureName == "anisometry") {
            // 矩形度/各向异性 - 使用最小外接矩形的长宽比
            cv::RotatedRect rotRect = cv::minAreaRect(contour);
            double w = rotRect.size.width;
            double h = rotRect.size.height;
            if (w > 0 && h > 0) {
                featureValue = (std::max)(w, h) / (std::min)(w, h);
            }
        }
        
        // 判断特征值是否在范围内
        if (featureValue >= minValue && featureValue <= maxValue) {
            cv::drawContours(result, std::vector<std::vector<cv::Point>>{contour}, 
                           0, cv::Scalar(255), cv::FILLED);
        }
    }
    
    return result;
}

// =============== 图像转换 ===============

cv::Mat OpenCVAlgorithm::convertToGreenWhite(const cv::Mat& mask)
{
    if (mask.empty()) return cv::Mat();
    
    cv::Mat m;
    if (mask.type() != CV_8U) {
        mask.convertTo(m, CV_8U);
    } else {
        m = mask.clone();
    }
    
    // 确保是二值图像
    cv::threshold(m, m, 127, 255, cv::THRESH_BINARY);
    
    cv::Mat result(m.rows, m.cols, CV_8UC3);
    
    for (int y = 0; y < m.rows; y++)
    {
        const uchar* mp = m.ptr<uchar>(y);
        cv::Vec3b* rp = result.ptr<cv::Vec3b>(y);
        for (int x = 0; x < m.cols; ++x)
        {
            if (mp[x] != 0)  // 在灰度范围内（白色 - 目标区域）
            {
                rp[x] = cv::Vec3b(0, 255, 0);  // 绿色
            }
            else  // 不在灰度范围内（黑色 - 背景区域）
            {
                rp[x] = cv::Vec3b(255, 255, 255);  // 白色
            }
        }
    }
    
    return result;
}
