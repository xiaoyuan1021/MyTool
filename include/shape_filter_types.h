#pragma once

#include <QString>
#include <QVector>
#include <HalconCpp.h>

/**
 * 形状特征类型
 *
 * 对应 Halcon 的 SelectShape 算子支持的特征
 */
enum class ShapeFeature
{
    Area,           // 面积
    Circularity,    // 圆度
    Width,          // 宽度（外接矩形）
    Height,         // 高度（外接矩形）
    Compactness,    // 紧凑度
    Convexity,      // 凸性
    RectangularityAnisometry, // 矩形度/各向异性
    Row,            // 中心行坐标
    Column          // 中心列坐标
};

/**
 * 获取特征的 Halcon 字符串名称
 */
inline const char* getFeatureName(ShapeFeature feature)
{
    switch (feature)
    {
    case ShapeFeature::Area:          return "area";
    case ShapeFeature::Circularity:   return "circularity";
    case ShapeFeature::Width:         return "width";
    case ShapeFeature::Height:        return "height";
    case ShapeFeature::Compactness:   return "compactness";
    case ShapeFeature::Convexity:     return "convexity";
    case ShapeFeature::RectangularityAnisometry: return "anisometry";
    case ShapeFeature::Row:           return "row";
    case ShapeFeature::Column:        return "column";
    default:                          return "area";
    }
}

/**
 * 获取特征的中文显示名称
 */
inline QString getFeatureDisplayName(ShapeFeature feature)
{
    switch (feature)
    {
    case ShapeFeature::Area:          return "面积";
    case ShapeFeature::Circularity:   return "圆度";
    case ShapeFeature::Width:         return "宽度";
    case ShapeFeature::Height:        return "高度";
    case ShapeFeature::Compactness:   return "紧凑度";
    case ShapeFeature::Convexity:     return "凸性";
    case ShapeFeature::RectangularityAnisometry: return "矩形度";
    case ShapeFeature::Row:           return "中心行";
    case ShapeFeature::Column:        return "中心列";
    default:                          return "未知";
    }
}

/**
 * 单个筛选条件
 */
struct FilterCondition
{
    ShapeFeature feature;   // 特征类型
    double minValue;        // 最小值
    double maxValue;        // 最大值
    bool enabled;           // 是否启用此条件

    FilterCondition()
        : feature(ShapeFeature::Area)
        , minValue(0.0)
        , maxValue(1e18)
        , enabled(false)
    {}

    FilterCondition(ShapeFeature f, double min, double max)
        : feature(f)
        , minValue(min)
        , maxValue(max)
        , enabled(true)
    {}

    // 判断是否有效（范围合理）
    bool isValid() const
    {
        return enabled && minValue >= 0 && maxValue >= minValue;
    }

    // 获取描述字符串
    QString toString() const
    {
        return QString("%1: [%2, %3]")
        .arg(getFeatureDisplayName(feature))
            .arg(minValue)
            .arg(maxValue);
    }
};

/**
 * 筛选模式
 */
enum class FilterMode
{
    And,  // 满足所有条件（逻辑与）
    Or    // 满足任意条件（逻辑或）
};

inline QString getFilterModeName(FilterMode mode)
{
    return (mode == FilterMode::And) ? "满足所有条件" : "满足任意条件";
}

/**
 * 筛选配置（包含多个条件）
 */
struct ShapeFilterConfig
{
    QVector<FilterCondition> conditions;  // 筛选条件列表
    FilterMode mode;                      // 筛选模式（AND/OR）
    bool enabled;                         // 是否启用筛选

    ShapeFilterConfig()
        : mode(FilterMode::And)
        , enabled(false)
    {}

    // 添加条件
    void addCondition(const FilterCondition& cond)
    {
        conditions.append(cond);
    }

    // 清空所有条件
    void clear()
    {
        conditions.clear();
        enabled = false;
    }

    // 获取启用的条件数量
    int getEnabledCount() const
    {
        int count = 0;
        for (const auto& cond : conditions) {
            if (cond.isValid()) count++;
        }
        return count;
    }

    // 是否有有效条件
    bool hasValidConditions() const
    {
        return enabled && getEnabledCount() > 0;
    }

    // 获取描述字符串
    QString toString() const
    {
        if (!hasValidConditions()) {
            return "未启用筛选";
        }

        QStringList condStrs;
        for (const auto& cond : conditions)
        {
            if (cond.isValid())
            {
                condStrs.append(cond.toString());
            }
        }

        QString separator = (mode == FilterMode::And) ? " 且 " : " 或 ";
        return condStrs.join(separator);
    }
};
