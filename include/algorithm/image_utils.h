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
    
};

#endif // IMAGE_UTILS_H
