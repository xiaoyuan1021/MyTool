#pragma once

#include <QStringList>
#include <QList>
#include "detection_type.h"

/**
 * @brief Tab配置结构
 * 定义每种检测类型需要显示的Tab列表
 */
struct TabConfig {
    QStringList tabNames;      // Tab名称列表
    QList<int> tabIndices;     // Tab索引列表（动态维护）
    
    TabConfig() {}
    
    TabConfig(const QStringList& names) : tabNames(names) {}
    
    /**
     * @brief 获取Blob分析的Tab配置
     */
    static TabConfig getBlobConfig() {
        return TabConfig({
            "图像",
            "增强",
            "过滤",
            "补正",
            "处理",
            "提取",
            "判定"
        });
    }
    
    /**
     * @brief 获取直线检测的Tab配置
     */
    static TabConfig getLineConfig() {
        return TabConfig({
            "图像",
            "增强",
            "直线"
        });
    }
    
    /**
     * @brief 获取条码识别的Tab配置
     */
    static TabConfig getBarcodeConfig() {
        return TabConfig({
            "图像",
            "增强",
            "条码"
        });
    }
    
    /**
     * @brief 获取视频源的Tab配置（简化版，视频源功能已集成到目标检测）
     */
    static TabConfig getVideoConfig() {
        return TabConfig({
            "图像",
            "视频",
            "增强",
            "过滤",
            "处理",
            "提取",
            "判定"
        });
    }
    
    /**
     * @brief 获取目标检测的Tab配置（包含视频源、预处理、检测、判定）
     */
    static TabConfig getObjectDetectionConfig() {
        return TabConfig({
            "图像",           // 图像显示
            "视频",           // 视频源配置（复用视频Tab）
            "增强",           // 图像预处理
            "目标检测",       // 检测方法配置
            "判定"            // 结果判定
        });
    }
    
    /**
     * @brief 获取视觉检测的通用Tab配置（适用于所有需要视频源的检测类型）
     */
    static TabConfig getVisionInspectionConfig() {
        return TabConfig({
            "图像",           // 图像显示
            "视频",           // 视频源配置
            "增强",           // 图像预处理
            "过滤",           // 过滤
            "处理",           // 处理
            "判定"            // 结果判定
        });
    }
    
    /**
     * @brief 根据检测类型获取Tab配置
     */
    static TabConfig getConfigForType(DetectionType type) {
        switch (type) {
            case DetectionType::Blob:
                return getBlobConfig();
            case DetectionType::Line:
                return getLineConfig();
            case DetectionType::Barcode:
                return getBarcodeConfig();
            case DetectionType::VideoSource:
                return getVideoConfig();
            case DetectionType::ObjectDetection:
                return getObjectDetectionConfig();
            default:
                return TabConfig();
        }
    }
};