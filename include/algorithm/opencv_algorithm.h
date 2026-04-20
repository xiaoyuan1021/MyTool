#pragma once

#include <opencv2/opencv.hpp>
#include <QVector>
#include <QPointF>

/**
 * OpenCV算法类 - 纯OpenCV实现的图像处理算法
 * 替代HalconAlgorithm，解决版权问题，支持嵌入式平台
 */

// 前向声明
struct AlgorithmStep;

/**
 * OpenCV算法类型枚举
 */
enum class OpenCVAlgoType
{
    OpeningCircle,      // 圆形开运算
    OpeningRect,        // 矩形开运算
    ClosingCircle,      // 圆形闭运算
    ClosingRect,        // 矩形闭运算
    DilationCircle,     // 圆形膨胀
    DilationRect,       // 矩形膨胀
    ErosionCircle,      // 圆形腐蚀
    ErosionRect,        // 矩形腐蚀
    Union,              // 联合
    Connection,         // 连通域
    FillUp,             // 填充孔洞
    ShapeTrans,         // 形状变换
    SelectShapeArea     // 面积筛选
};

class OpenCVAlgorithm
{
public:
    OpenCVAlgorithm();

    // ========== 核心调度方法 ==========

    // 根据AlgorithmStep执行对应算法
    static cv::Mat execute(const cv::Mat& region, const AlgorithmStep& step);

    // ========== 形态学操作 ==========

    // 开运算（圆形核）
    static cv::Mat openingCircle(const cv::Mat& region, double radius);
    
    // 开运算（矩形核）
    static cv::Mat openingRectangle(const cv::Mat& region, int width, int height);
    
    // 闭运算（圆形核）
    static cv::Mat closingCircle(const cv::Mat& region, double radius);
    
    // 闭运算（矩形核）
    static cv::Mat closingRectangle(const cv::Mat& region, int width, int height);
    
    // 膨胀（圆形核）
    static cv::Mat dilateCircle(const cv::Mat& region, double radius);
    
    // 膨胀（矩形核）
    static cv::Mat dilateRectangle(const cv::Mat& region, int width, int height);
    
    // 腐蚀（圆形核）
    static cv::Mat erodeCircle(const cv::Mat& region, double radius);
    
    // 腐蚀（矩形核）
    static cv::Mat erodeRectangle(const cv::Mat& region, int width, int height);

    // ========== 连通域操作 ==========

    // 连通域标记
    static cv::Mat connection(const cv::Mat& region);
    
    // 合并所有连通域
    static cv::Mat union1(const cv::Mat& region);

    // ========== 填充操作 ==========

    // 填充孔洞
    static cv::Mat fillUpHoles(const cv::Mat& region);

    // ========== 形状变换 ==========

    // 形状变换类型枚举
    enum class ShapeTransType {
        Convex,         // 凸包
        Rectangle1,     // 最小外接矩形（轴对齐）
        Rectangle2,     // 最小外接矩形（旋转）
        InnerCircle,    // 最大内接圆
        OuterCircle,    // 最小外接圆
        Ellipse         // 椭圆拟合
    };

    // 形状变换
    static cv::Mat shapeTrans(const cv::Mat& region, ShapeTransType type);

    // ========== 区域筛选 ==========

    // 按面积筛选连通域
    static cv::Mat selectShapeArea(const cv::Mat& region, double minArea, double maxArea);

    // 区域特征结构体
    struct RegionFeature {
        int index;
        double area;
        double centerX;
        double centerY;
        double circularity;
        double width;
        double height;
    };

    // 分析多边形区域内的连通域
    static QVector<RegionFeature> analyzeRegionsInPolygon(
        const QVector<QPointF>& polygon,
        const cv::Mat& processedImage
    );

    // ========== 区域特征计算 ==========

    // 计算区域面积
    static double calculateArea(const cv::Mat& region);
    
    // 计算区域圆度
    static double calculateRegionCircularity(const cv::Mat& region);
    
    // 计算区域周长
    static double calculatePerimeter(const cv::Mat& region);
    
    // 计算单个区域的完整特征
    static RegionFeature calculateRegionFeature(const cv::Mat& region, int index = 1);
    
    // ========== 新增特征计算函数 ==========
    
    // 计算紧凑度 (4π*面积/周长²)
    static double calculateCompactness(const std::vector<cv::Point>& contour);
    
    // 计算凸性 (面积/凸包面积)
    static double calculateConvexity(const std::vector<cv::Point>& contour);
    
    // 计算矩形度 (面积/最小外接矩形面积)
    static double calculateRectangularity(const std::vector<cv::Point>& contour);
    
    // 按特征筛选连通域
    static cv::Mat selectShapeByFeature(const cv::Mat& region, const QString& featureName,
                                        double minValue, double maxValue);

    // ========== 图像转换 ==========

    // 将mask转换为绿白图像（目标区域为绿色，背景为白色）
    static cv::Mat convertToGreenWhite(const cv::Mat& mask);

private:
    // 创建圆形结构元素
    static cv::Mat createCircleKernel(int radius);
    
    // 计算圆形度（基于轮廓）
    static double calculateCircularity(const std::vector<cv::Point>& contour);
};
