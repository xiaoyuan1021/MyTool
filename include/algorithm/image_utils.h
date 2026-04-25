#ifndef IMAGE_UTILS_H
#define IMAGE_UTILS_H

#include <opencv2/opencv.hpp>
#include <QImage>
#include <QLabel>

class ImageUtils
{
public:
    //禁止实例化
    ImageUtils()=delete;
    
    static QImage matToQImage(const cv::Mat &mat);
    static cv::Mat qImageToMat(const QImage &image, bool clone = true);

    static cv::Mat makeStructElement(int ksize);
    static QRect mapLabelToImage(const QRect& rect,const cv::Mat& img,QLabel* label);

    /// 将 QRectF 映射到图像边界内的 cv::Rect；无效时返回空 Rect
    static cv::Rect mapRoiToCvRect(const QRectF& roiRect, int imageCols, int imageRows);
    
};

#endif // IMAGE_UTILS_H
