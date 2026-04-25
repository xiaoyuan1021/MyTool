#pragma once

#include <QString>
#include <QMap>
#include <QList>
#include <QVector>

#include <opencv2/opencv.hpp>

#include "image_utils.h"


struct AlgorithmStep
{
    QString name;                   // 算法显示名称
    QString type;                   // 算法类型标识
    QMap <QString,QVariant> params; // 参数字典
    bool enabled=true;              // 是否启用
    QString description;            // 算法说明
};

class ImageProcessor
{
public:
    ImageProcessor();

    cv::Mat convertColorSpace(const cv::Mat&src,const QString& mode);

    cv::Mat executeAlgorithmQueue(const cv::Mat&src,const QVector<AlgorithmStep>&queue);

    cv::Mat adjustParameter(const cv::Mat &src, int brightness, double contrast,double gamma,double sharpen);

    cv::Mat filterRGB(const cv::Mat& src,
                      int rLow, int rHigh,
                      int gLow, int gHigh,
                      int bLow, int bHigh);
    cv::Mat filterHSV(const cv::Mat& src,
                      int hLow, int hHigh,
                      int sLow, int sHigh,
                      int vLow, int vHigh);

};

