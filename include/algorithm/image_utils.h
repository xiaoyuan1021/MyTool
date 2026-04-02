#ifndef IMAGE_UTILS_H
#define IMAGE_UTILS_H

#include <opencv2/opencv.hpp>
#include <HalconCpp.h>
#include <QImage>
#include <QLabel>

using namespace cv;
using namespace HalconCpp;

class ImageUtils
{
public:
    //禁止实例化
    ImageUtils()=delete;
    static QImage Mat2Qimage(const cv::Mat &mat);

    static Mat Qimage2Mat(const QImage &image, bool clone = true);


    static HObject MatToHImage(const cv::Mat& cv_img);
    static Mat HImageToMat(HalconCpp::HObject &H_img);
    static Mat HObjectToMat(const HalconCpp::HObject& hObj, int width, int height);
    static Mat makeStructElement(int ksize);
    static QRect mapLabelToImage(const QRect& rect,const Mat& img,QLabel* label);
    static HRegion MatToHRegion(const cv::Mat& binary);
    static Mat HRegionToMat(const HRegion& region, int width, int height);
    static HImage Mat2HImage(const cv::Mat& mat);
};

#endif // IMAGE_UTILS_H
