#pragma once

/**
 * 形状筛选相关类型聚合头文件（合并版）
 * 
 * 将 shape_feature.h、filter_condition.h、shape_filter_config.h
 * 合并为单一头文件，减少文件碎片化。
 * 
 * 原子文件保留为转发头文件，保持向后兼容性。
 */

#include <QString>
#include <QStringList>
#include <QVector>

/**
 * 形状特征类型枚举
 *
 * 用于筛选和描述连通区域几何属性
 */
enum class ShapeFeature
{
    Area,                        // 面积
    Circularity,                 // 圆度
    Width,                       // 宽度（外接矩形）
    Height,                      // 高度（外接矩形）
    Compactness,                 // 紧凑度
    Convexity,                   // 凸性
    RectangularityAnisometry     // 矩形度/各向异性
};

/**
 * 获取特征的字符串名称（OpenCV 兼容命名）
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
    default:                          return "未知";
    }
}

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

/**
 * 筛选模式枚举
 * 
 * 定义多个条件之间的逻辑关系
 */
enum class ShapeFilterLogicMode
{
    And,  // 满足所有条件（逻辑与）
    Or    // 满足任意条件（逻辑或）
};

/**
 * 获取筛选模式名称
 */
inline QString getFilterModeName(ShapeFilterLogicMode mode)
{
    return (mode == ShapeFilterLogicMode::And) ? "满足所有条件" : "满足任意条件";
}

/**
 * 形状筛选配置结构体
 * 
 * 包含多个筛选条件和筛选模式
 */
struct ShapeFilterConfig
{
    QVector<FilterCondition> conditions;  // 筛选条件列表
    ShapeFilterLogicMode mode;            // 筛选模式（AND/OR）

    /**
     * 默认构造函数
     */
    ShapeFilterConfig()
        : mode(ShapeFilterLogicMode::And)
    {}

    /**
     * 添加筛选条件
     */
    void addCondition(const FilterCondition& cond)
    {
        conditions.append(cond);
    }

    /**
     * 清空所有条件
     */
    void clear()
    {
        conditions.clear();
    }

    /**
     * 获取有效条件数量
     */
    int getEnabledCount() const
    {
        int count = 0;
        for (const auto& cond : conditions) {
            if (cond.isValid()) count++;
        }
        return count;
    }

    /**
     * 是否有有效条件
     */
    bool hasValidConditions() const
    {
        return getEnabledCount() > 0;
    }

    /**
     * 获取描述字符串
     */
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

        QString separator = (mode == ShapeFilterLogicMode::And) ? " 且 " : " 或 ";
        return condStrs.join(separator);
    }

    /**
     * 设置筛选模式
     */
    void setMode(ShapeFilterLogicMode newMode)
    {
        mode = newMode;
    }

    /**
     * 重置为默认配置
     */
    void reset()
    {
        conditions.clear();
        mode = ShapeFilterLogicMode::And;
    }
};

// 向后兼容的别名
using FilterMode = ShapeFilterLogicMode;