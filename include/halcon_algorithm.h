#ifndef HALCON_ALGORITHM_H
#define HALCON_ALGORITHM_H


#include <HalconCpp.h>
#include <QDialog>

#include <QComboBox>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QLabel>
#include <QFormLayout>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QStackedWidget>
#include <QGroupBox>
#include <QVector>
#include <QPointF>
#include "opencv2/opencv.hpp"
#include "pipeline.h"

using namespace HalconCpp;

// ⭐ 前向声明，替代 #include "imageprocessor.h"
struct AlgorithmStep;

class HalconAlgorithm
{
public:
    HalconAlgorithm();

    HRegion execute(const HRegion& region, const AlgorithmStep& step);
    //开闭运算
    HRegion openingCircle(const HRegion& region, double radius);
    HRegion openingRectangle(const HRegion& region, Hlong width,Hlong height);
    HRegion closeingCircle(const HRegion& region, double radius);
    HRegion closeingRectangle(const HRegion& region, Hlong width,Hlong height);
    //膨胀腐蚀
    HRegion dilateCircle(const HRegion& region, double radius);
    HRegion dilateRectangle(const HRegion& region, Hlong width,Hlong height);
    HRegion erodeCircle(const HRegion& region, double radius);
    HRegion erodeRectangle(const HRegion& region, Hlong width,Hlong height);
    //联合连通
    HRegion Union(const HRegion& region);
    HRegion Connection(const HRegion& region);
    //填充孔洞
    HRegion fillUpHoles(const HRegion& region);
    //形状变换
    HRegion shapeTrans(const HRegion& region,const HString& type);

    HRegion selectShapeArea(const HRegion& region,double minArea,double maxArea);

    QVector<RegionFeature> analyzeRegionsInPolygon(
        const QVector<QPointF>& polygon,
        const cv::Mat& processedImage
        );

};

#endif // HALCON_ALGORITHM_H
