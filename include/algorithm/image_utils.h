#ifndef IMAGE_UTILS_H
#define IMAGE_UTILS_H

#include <opencv2/opencv.hpp>
// ⚠️ 已注释：不再需要 Halcon 头文件
// #include <HalconCpp.h>
#include <QImage>
#include <QLabel>

// 修复ACCESS_MASK冲突：不使用using namespace cv
// using namespace cv;
// ⚠️ 已注释：不再需要 Halcon 命名空间
// using namespace HalconCpp;

class ImageUtils
{
public:
    //禁止实例化
    ImageUtils()=delete;
    
    static QImage matToQImage(const cv::Mat &mat);
    static cv::Mat qImageToMat(const QImage &image, bool clone = true);

    static cv::Mat makeStructElement(int ksize);
    static QRect mapLabelToImage(const QRect& rect,const cv::Mat& img,QLabel* label);
    
    // ⚠️ 已注释：Halcon 转换函数（不再需要）
    // static Mat HImageToMat(HalconCpp::HObject &H_img);
    // static Mat HObjectToMat(const HalconCpp::HObject& hObj, int width, int height);
    // static HRegion MatToHRegion(const cv::Mat& binary);
    // static Mat HRegionToMat(const HRegion& region, int width, int height);
    // static HImage matToHImage(const cv::Mat& mat);
};

#endif // IMAGE_UTILS_H
