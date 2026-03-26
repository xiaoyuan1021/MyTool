#pragma once

#include <QString>

/**
 * 形状特征类型枚举
 * 
 * 对应Halcon的SelectShape算子支持的特征
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
 * 获取特征的Halcon字符串名称
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