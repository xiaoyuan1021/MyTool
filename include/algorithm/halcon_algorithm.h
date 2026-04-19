#pragma once


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

// ⭐ 前向声明，替代 #include "imageprocessor.h"
struct AlgorithmStep;

class HalconAlgorithm
{
public:
    HalconAlgorithm();

    HalconCpp::HRegion execute(const HalconCpp::HRegion& region, const AlgorithmStep& step);
    //开闭运算
    HalconCpp::HRegion openingCircle(const HalconCpp::HRegion& region, double radius);
    HalconCpp::HRegion openingRectangle(const HalconCpp::HRegion& region, Hlong width, Hlong height);
    HalconCpp::HRegion closingCircle(const HalconCpp::HRegion& region, double radius);
    HalconCpp::HRegion closingRectangle(const HalconCpp::HRegion& region, Hlong width, Hlong height);
    //膨胀腐蚀
    HalconCpp::HRegion dilateCircle(const HalconCpp::HRegion& region, double radius);
    HalconCpp::HRegion dilateRectangle(const HalconCpp::HRegion& region, Hlong width, Hlong height);
    HalconCpp::HRegion erodeCircle(const HalconCpp::HRegion& region, double radius);
    HalconCpp::HRegion erodeRectangle(const HalconCpp::HRegion& region, Hlong width, Hlong height);
    //联合连通
    HalconCpp::HRegion union1(const HalconCpp::HRegion& region);
    HalconCpp::HRegion connection(const HalconCpp::HRegion& region);
    //填充孔洞
    HalconCpp::HRegion fillUpHoles(const HalconCpp::HRegion& region);
    //形状变换
    HalconCpp::HRegion shapeTrans(const HalconCpp::HRegion& region, const HalconCpp::HString& type);

    HalconCpp::HRegion selectShapeArea(const HalconCpp::HRegion& region, double minArea, double maxArea);

    // QVector<RegionFeature> analyzeRegionsInPolygon(
    //     const QVector<QPointF>& polygon,
    //     const cv::Mat& processedImage
    //     );

};

