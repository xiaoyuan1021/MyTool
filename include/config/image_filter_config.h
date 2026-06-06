#pragma once

#include <QJsonObject>

/// 滤波去噪类型
enum class FilterDenoiseType
{
    Gaussian = 0,   // 高斯滤波
    Median,         // 中值滤波
    Bilateral,      // 双边滤波
    Morphology      // 形态学滤波
};

/// 形态学操作类型
enum class MorphologyOpType
{
    Open = 0,   // 开运算（去小颗粒）
    Close,      // 闭运算（填小孔）
    Erode,      // 腐蚀
    Dilate      // 膨胀
};

/**
 * @brief 滤波去噪配置
 *
 * 支持高斯滤波、中值滤波、双边滤波、形态学滤波四种类型
 */
struct ImageFilterConfig
{
    FilterDenoiseType filterType = FilterDenoiseType::Gaussian;

    // 高斯滤波参数
    int gaussianKernelSize = 3;     // 核大小（奇数）
    double gaussianSigmaX = 1.0;    // X方向sigma
    double gaussianSigmaY = 0.0;    // Y方向sigma（0=与sigmaX相同）

    // 中值滤波参数
    int medianKernelSize = 3;       // 核大小（奇数）

    // 双边滤波参数
    int bilateralD = 9;             // 像素邻域直径
    double bilateralSigmaColor = 75.0;  // 颜色空间sigma
    double bilateralSigmaSpace = 75.0;  // 坐标空间sigma

    // 形态学滤波参数
    MorphologyOpType morphologyOp = MorphologyOpType::Open;
    int morphologyKernelSize = 3;   // 核大小
    int morphologyIterations = 1;   // 迭代次数

    bool operator==(const ImageFilterConfig& o) const {
        return filterType == o.filterType &&
               gaussianKernelSize == o.gaussianKernelSize &&
               gaussianSigmaX == o.gaussianSigmaX &&
               gaussianSigmaY == o.gaussianSigmaY &&
               medianKernelSize == o.medianKernelSize &&
               bilateralD == o.bilateralD &&
               bilateralSigmaColor == o.bilateralSigmaColor &&
               bilateralSigmaSpace == o.bilateralSigmaSpace &&
               morphologyOp == o.morphologyOp &&
               morphologyKernelSize == o.morphologyKernelSize &&
               morphologyIterations == o.morphologyIterations;
    }

    QJsonObject toJson() const {
        QJsonObject obj;
        obj["filterType"] = static_cast<int>(filterType);
        obj["gaussianKernelSize"] = gaussianKernelSize;
        obj["gaussianSigmaX"] = gaussianSigmaX;
        obj["gaussianSigmaY"] = gaussianSigmaY;
        obj["medianKernelSize"] = medianKernelSize;
        obj["bilateralD"] = bilateralD;
        obj["bilateralSigmaColor"] = bilateralSigmaColor;
        obj["bilateralSigmaSpace"] = bilateralSigmaSpace;
        obj["morphologyOp"] = static_cast<int>(morphologyOp);
        obj["morphologyKernelSize"] = morphologyKernelSize;
        obj["morphologyIterations"] = morphologyIterations;
        return obj;
    }

    void fromJson(const QJsonObject& obj) {
        filterType = static_cast<FilterDenoiseType>(obj["filterType"].toInt(0));
        gaussianKernelSize = obj["gaussianKernelSize"].toInt(3);
        gaussianSigmaX = obj["gaussianSigmaX"].toDouble(1.0);
        gaussianSigmaY = obj["gaussianSigmaY"].toDouble(0.0);
        medianKernelSize = obj["medianKernelSize"].toInt(3);
        bilateralD = obj["bilateralD"].toInt(9);
        bilateralSigmaColor = obj["bilateralSigmaColor"].toDouble(75.0);
        bilateralSigmaSpace = obj["bilateralSigmaSpace"].toDouble(75.0);
        morphologyOp = static_cast<MorphologyOpType>(obj["morphologyOp"].toInt(0));
        morphologyKernelSize = obj["morphologyKernelSize"].toInt(3);
        morphologyIterations = obj["morphologyIterations"].toInt(1);
    }
};
