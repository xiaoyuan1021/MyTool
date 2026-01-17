#ifndef IMAGE_PROCESSOR_H
#define IMAGE_PROCESSOR_H

#include <QString>
#include <QMap>
#include <QList>
#include <QImage>
#include <QDebug>
#include <QLabel>
#include <QVector>
#include <QDir>

#include <opencv2/opencv.hpp>
#include <HalconCpp.h>

#include "image_utils.h"
#include "halcon_algorithm.h"


using namespace cv;

// Halcon算法类型枚举
enum class HalconAlgoType
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


struct AlgorithmStep
{
    QString name;                   // 算法显示名称
    QString type;                   // 算法类型标识
    QMap <QString,QVariant> params; // 参数字典
    bool enabled=true;              // 是否启用
    QString description;            // 算法说明
};

class imageprocessor
{
public:
    imageprocessor();

    Mat convertColorSpace(const Mat&src,const QString& mode);

    Mat executeAlgorithmQueue(const Mat&src,const QVector<AlgorithmStep>&queue);

    Mat adjustParameter(const Mat &src, int brightness, double contrast,double gamma,double sharpen);

    //Mat extractFeatures(const Mat& src);

};

#endif // IMAGE_PROCESSOR_H
