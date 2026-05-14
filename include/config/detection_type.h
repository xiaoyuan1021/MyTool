#pragma once

#include <QString>
#include <QDateTime>
#include <QRandomGenerator>

/**
 * @brief 生成唯一ID
 * @param prefix 前缀（如 "roi" 或 "det"）
 * @return 唯一ID字符串
 */
inline QString generateUniqueId(const QString& prefix) {
    return QString("%1_%2_%3")
        .arg(prefix)
        .arg(QDateTime::currentMSecsSinceEpoch())
        .arg(QRandomGenerator::global()->bounded(10000));
}

/**
 * @brief 检测类型枚举
 */
enum class DetectionType {
    Barcode,        // 条码检测
    Template,       // 模板匹配
    Line,           // 直线检测
    Blob,           // Blob分析
    VideoSource,     // 视频源
    ObjectDetection,  // 目标检测
    VideoDetection,  // 视频检测（视频源 + 目标检测）
    Custom           // 自定义Pipeline
};

/**
 * @brief 将检测类型转换为字符串
 */
inline QString detectionTypeToString(DetectionType type) {
    switch (type) {
        case DetectionType::Barcode: return "条码检测";
        case DetectionType::Template: return "模板匹配";
        case DetectionType::Line: return "直线检测";
        case DetectionType::Blob: return "Blob分析";
        case DetectionType::VideoSource: return "视频源";
        case DetectionType::ObjectDetection: return "目标检测";
        case DetectionType::VideoDetection: return "视频检测";
        case DetectionType::Custom: return "自定义Pipeline";
        default: return "未知类型";
    }
}

/**
 * @brief 将字符串转换为检测类型
 */
inline DetectionType stringToDetectionType(const QString& str) {
    if (str == "条码检测") return DetectionType::Barcode;
    if (str == "模板匹配") return DetectionType::Template;
    if (str == "直线检测") return DetectionType::Line;
    if (str == "Blob分析") return DetectionType::Blob;
    if (str == "视频源") return DetectionType::VideoSource;
    if (str == "目标检测") return DetectionType::ObjectDetection;
    if (str == "视频检测") return DetectionType::VideoDetection;
    if (str == "自定义Pipeline") return DetectionType::Custom;
    return DetectionType::Blob; // 默认值
}

/**
 * @brief 视频源类型枚举
 */
enum class VideoSourceType {
    None,       // 无视频源（使用静态图像）
    Camera,     // 相机
    VideoFile,  // 视频文件
    ImageDir    // 图像目录
};

/**
 * @brief 将VideoSourceType转换为字符串
 */
inline QString videoSourceTypeToString(VideoSourceType type) {
    switch (type) {
        case VideoSourceType::None: return "无";
        case VideoSourceType::Camera: return "相机";
        case VideoSourceType::VideoFile: return "视频文件";
        case VideoSourceType::ImageDir: return "图像目录";
        default: return "未知";
    }
}

/**
 * @brief 将字符串转换为VideoSourceType
 */
inline VideoSourceType stringToVideoSourceType(const QString& str) {
    if (str == "无") return VideoSourceType::None;
    if (str == "相机") return VideoSourceType::Camera;
    if (str == "视频文件") return VideoSourceType::VideoFile;
    if (str == "图像目录") return VideoSourceType::ImageDir;
    return VideoSourceType::None;
}