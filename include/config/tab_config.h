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
            "颜色过滤",
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
     * @brief 获取视频源的Tab配置
     */
    static TabConfig getVideoConfig() {
        return TabConfig({
            "图像",
            "视频",
            "增强",
            "颜色过滤",
            "处理",
            "提取",
            "判定"
        });
    }
    
    /**
     * @brief 获取目标检测的Tab配置
     */
    static TabConfig getObjectDetectionConfig() {
        return TabConfig({
            "图像",
            "增强",
            "目标检测"
        });
    }
    
    /**
     * @brief 获取视频检测的Tab配置（视频源 + 目标检测）
     */
    static TabConfig getVideoDetectionConfig() {
        return TabConfig({
            "视频",
            "目标检测"
        });
    }
    
    /**
     * @brief 获取自定义Pipeline的Tab配置
     * 初始只显示"步骤"Tab，用户配置后动态加载其他Tab
     */
    static TabConfig getCustomConfig() {
        return TabConfig({
            "步骤"
        });
    }
    
    /**
     * @brief 获取OCR识别的Tab配置
     */
    static TabConfig getOcrConfig() {
        return TabConfig({
            "图像",
            "增强",
            "文字识别"
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
            case DetectionType::VideoDetection:
                return getVideoDetectionConfig();
            case DetectionType::Custom:
                return getCustomConfig();
            case DetectionType::Ocr:
                return getOcrConfig();
            default:
                return TabConfig();
        }
    }
};