#pragma once

#include <QString>
#include "shape_feature.h"

/**
 * 单个筛选条件结构体
 * 
 * 定义一个形状特征的筛选范围
 */
struct FilterCondition
{
    ShapeFeature feature;   // 特征类型
    double minValue;        // 最小值
    double maxValue;        // 最大值

    /**
     * 默认构造函数
     */
    FilterCondition()
        : feature(ShapeFeature::Area)
        , minValue(0.0)
        , maxValue(1e18)
    {}

    /**
     * 带参数的构造函数
     */
    FilterCondition(ShapeFeature f, double min, double max)
        : feature(f)
        , minValue(min)
        , maxValue(max)
    {}

    /**
     * 判断条件是否有效（范围合理）
     */
    bool isValid() const
    {
        return minValue >= 0 && maxValue >= minValue;
    }

    /**
     * 获取描述字符串
     */
    QString toString() const
    {
        return QString("%1: [%2, %3]")
            .arg(getFeatureDisplayName(feature))
            .arg(minValue)
            .arg(maxValue);
    }

    /**
     * 重置为默认值
     */
    void reset()
    {
        feature = ShapeFeature::Area;
        minValue = 0.0;
        maxValue = 1e18;
    }
};