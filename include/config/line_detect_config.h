#pragma once

#include <QJsonObject>
#include <opencv2/core.hpp>
#include "config/enhance_config.h"

/**
 * @brief 直线检测与参考线匹配配置（统一版本）
 *
 * 合并了原 LineDetectConfig（PipelineConfig 用）和
 * LineDetectionConfig（DetectionItem 用）。
 * 对应 LineTabWidget 的参数。
 */
struct LineDetectConfig
{
    EnhanceConfig enhance;  ///< 图像增强参数（独立实例）

    int algorithm = 0;      // 0=HoughP, 1=LSD, 2=EDline
    double rho = 1.0;
    double theta = CV_PI / 180.0;
    int threshold = 50;
    double minLength = 30.0;
    double maxGap = 10.0;
    bool enabled = false;

    // 参考线匹配参数
    bool enableReferenceLineMatch = false;
    cv::Point2f referenceLineStart;
    cv::Point2f referenceLineEnd;
    bool referenceLineValid = false;
    double angleThreshold = 15.0;      // 角度容差（度）
    double distanceThreshold = 50.0;   // 距离容差（像素）
    int searchRegionWidth = 100;       // 搜索区域宽度（像素）

    QJsonObject toJson() const;
    void fromJson(const QJsonObject& obj);
};
