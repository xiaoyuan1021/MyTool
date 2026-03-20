#pragma once

#include <QString>
#include <opencv2/opencv.hpp>

/**
 * 区域特征数据结构
 * 
 * 用于存储图像处理后提取的区域特征信息
 * 这个结构体是独立的数据类型，不依赖Pipeline
 */
struct RegionFeature
{
    int index = 0;              // 区域索引
    double area = 0.0;          // 面积
    double circularity = 0.0;   // 圆度
    double centerX = 0.0;       // 中心X坐标
    double centerY = 0.0;       // 中心Y坐标
    double width = 0.0;         // 宽度
    double height = 0.0;        // 高度
    cv::Rect bbox;              // 外接矩形

    /**
     * 转换为字符串描述
     */
    QString toString() const
    {
        return QString::asprintf(
            "区域 %d: 面积=%.1f, 圆度=%.3f, 中心=(%.1f,%.1f), 尺寸=%.1f×%.1f",
            index, area, circularity, centerX, centerY, width, height
        );
    }

    /**
     * 检查特征是否有效
     */
    bool isValid() const
    {
        return area > 0 && width > 0 && height > 0;
    }

    /**
     * 重置为默认值
     */
    void reset()
    {
        index = 0;
        area = 0.0;
        circularity = 0.0;
        centerX = 0.0;
        centerY = 0.0;
        width = 0.0;
        height = 0.0;
        bbox = cv::Rect();
    }
};