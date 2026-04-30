#pragma once

#include <QString>
#include <QJsonObject>
#include <QJsonArray>
#include "detection_type.h"
#include "detection_config_types.h"

/**
 * @brief 视觉检测项配置 - 组合视频源 + 预处理 + 检测方法 + 判定
 * 
 * 这是重构后的统一配置结构，将视频流和目标检测合并为一个完整的检测流水线。
 */
struct VisionInspectionConfig {
    // ===== 视频源配置 =====
    enum class VideoSourceType {
        None,       // 无视频源（使用静态图像）
        Camera,     // 相机
        VideoFile,  // 视频文件
        ImageDir    // 图像目录
    };
    
    VideoSourceType videoSourceType = VideoSourceType::None;
    QString videoFilePath;          // 视频文件路径
    QString imageDirectory;         // 图像目录路径
    int cameraIndex = 0;            // 相机索引
    
    // 播放控制参数
    bool autoPlay = false;          // 自动播放
    double playbackSpeed = 1.0;     // 播放速度 (0.5 ~ 2.0)
    bool loopPlayback = false;      // 循环播放
    
    // 帧处理参数
    bool enableFrameSkipping = false;   // 是否启用帧跳跃
    int frameSkipInterval = 1;          // 帧跳跃间隔
    
    // ===== 图像预处理配置 =====
    struct PreprocessConfig {
        int brightness = 0;          // 亮度 (-100 ~ 100)
        int contrast = 100;          // 对比度 (0 ~ 200)
        int gamma = 100;             // 伽马值 (10 ~ 300)
        int sharpen = 100;           // 锐化 (0 ~ 200)
        
        // 去噪参数
        bool enableDenoise = false;
        int denoiseStrength = 3;
        
        // ROI配置
        bool enableRoi = false;
        int roiX = 0;
        int roiY = 0;
        int roiWidth = 100;
        int roiHeight = 100;
        
        QJsonObject toJson() const {
            QJsonObject obj;
            obj["brightness"] = brightness;
            obj["contrast"] = contrast;
            obj["gamma"] = gamma;
            obj["sharpen"] = sharpen;
            obj["enableDenoise"] = enableDenoise;
            obj["denoiseStrength"] = denoiseStrength;
            obj["enableRoi"] = enableRoi;
            obj["roiX"] = roiX;
            obj["roiY"] = roiY;
            obj["roiWidth"] = roiWidth;
            obj["roiHeight"] = roiHeight;
            return obj;
        }
        
        void fromJson(const QJsonObject& obj) {
            brightness = obj["brightness"].toInt(0);
            contrast = obj["contrast"].toInt(100);
            gamma = obj["gamma"].toInt(100);
            sharpen = obj["sharpen"].toInt(100);
            enableDenoise = obj["enableDenoise"].toBool(false);
            denoiseStrength = obj["denoiseStrength"].toInt(3);
            enableRoi = obj["enableRoi"].toBool(false);
            roiX = obj["roiX"].toInt(0);
            roiY = obj["roiY"].toInt(0);
            roiWidth = obj["roiWidth"].toInt(100);
            roiHeight = obj["roiHeight"].toInt(100);
        }
    };
    
    PreprocessConfig preprocess;
    
    // ===== 检测方法配置 =====
    DetectionType detectionMethod = DetectionType::Blob;
    QJsonObject detectionConfig;    // 检测方法的专属配置
    
    // ===== 判定配置 =====
    struct JudgeConfig {
        bool enabled = true;                // 是否启用判定
        int judgeType = 0;                  // 判定类型 (0: 数量, 1: 面积, 2: 位置, 3: 自定义)
        
        // 数量判定
        int minCount = 0;                   // 最小数量
        int maxCount = 100;                 // 最大数量
        
        // 面积判定
        double minTotalArea = 0;            // 最小总面积
        double maxTotalArea = 1000000;      // 最大面积
        
        // 通过/失败条件
        bool passWhenInRange = true;        // 在范围内为通过
        
        QJsonObject toJson() const {
            QJsonObject obj;
            obj["enabled"] = enabled;
            obj["judgeType"] = judgeType;
            obj["minCount"] = minCount;
            obj["maxCount"] = maxCount;
            obj["minTotalArea"] = minTotalArea;
            obj["maxTotalArea"] = maxTotalArea;
            obj["passWhenInRange"] = passWhenInRange;
            return obj;
        }
        
        void fromJson(const QJsonObject& obj) {
            enabled = obj["enabled"].toBool(true);
            judgeType = obj["judgeType"].toInt(0);
            minCount = obj["minCount"].toInt(0);
            maxCount = obj["maxCount"].toInt(100);
            minTotalArea = obj["minTotalArea"].toDouble(0);
            maxTotalArea = obj["maxTotalArea"].toDouble(1000000);
            passWhenInRange = obj["passWhenInRange"].toBool(true);
        }
    };
    
    JudgeConfig judge;
    
    // ===== 显示配置 =====
    struct DisplayConfig {
        bool showOverlay = true;            // 是否显示叠加层
        bool showBoundingBox = true;        // 是否显示边界框
        bool showLabels = true;             // 是否显示标签
        bool showConfidence = true;         // 是否显示置信度
        int lineWidth = 2;                  // 线宽
        
        QJsonObject toJson() const {
            QJsonObject obj;
            obj["showOverlay"] = showOverlay;
            obj["showBoundingBox"] = showBoundingBox;
            obj["showLabels"] = showLabels;
            obj["showConfidence"] = showConfidence;
            obj["lineWidth"] = lineWidth;
            return obj;
        }
        
        void fromJson(const QJsonObject& obj) {
            showOverlay = obj["showOverlay"].toBool(true);
            showBoundingBox = obj["showBoundingBox"].toBool(true);
            showLabels = obj["showLabels"].toBool(true);
            showConfidence = obj["showConfidence"].toBool(true);
            lineWidth = obj["lineWidth"].toInt(2);
        }
    };
    
    DisplayConfig display;
    
    // ===== 序列化方法 =====
    
    QJsonObject toJson() const {
        QJsonObject obj;
        
        // 视频源
        QJsonObject videoSource;
        videoSource["type"] = static_cast<int>(videoSourceType);
        videoSource["videoFilePath"] = videoFilePath;
        videoSource["imageDirectory"] = imageDirectory;
        videoSource["cameraIndex"] = cameraIndex;
        videoSource["autoPlay"] = autoPlay;
        videoSource["playbackSpeed"] = playbackSpeed;
        videoSource["loopPlayback"] = loopPlayback;
        videoSource["enableFrameSkipping"] = enableFrameSkipping;
        videoSource["frameSkipInterval"] = frameSkipInterval;
        obj["videoSource"] = videoSource;
        
        // 预处理
        obj["preprocess"] = preprocess.toJson();
        
        // 检测方法
        QJsonObject method;
        method["type"] = detectionTypeToString(detectionMethod);
        method["config"] = detectionConfig;
        obj["detectionMethod"] = method;
        
        // 判定
        obj["judge"] = judge.toJson();
        
        // 显示
        obj["display"] = display.toJson();
        
        return obj;
    }
    
    void fromJson(const QJsonObject& obj) {
        // 视频源
        QJsonObject videoSource = obj["videoSource"].toObject();
        videoSourceType = static_cast<VideoSourceType>(videoSource["type"].toInt(0));
        videoFilePath = videoSource["videoFilePath"].toString();
        imageDirectory = videoSource["imageDirectory"].toString();
        cameraIndex = videoSource["cameraIndex"].toInt(0);
        autoPlay = videoSource["autoPlay"].toBool(false);
        playbackSpeed = videoSource["playbackSpeed"].toDouble(1.0);
        loopPlayback = videoSource["loopPlayback"].toBool(false);
        enableFrameSkipping = videoSource["enableFrameSkipping"].toBool(false);
        frameSkipInterval = videoSource["frameSkipInterval"].toInt(1);
        
        // 预处理
        preprocess.fromJson(obj["preprocess"].toObject());
        
        // 检测方法
        QJsonObject method = obj["detectionMethod"].toObject();
        detectionMethod = stringToDetectionType(method["type"].toString("Blob分析"));
        detectionConfig = method["config"].toObject();
        
        // 判定
        judge.fromJson(obj["judge"].toObject());
        
        // 显示
        display.fromJson(obj["display"].toObject());
    }
};

/**
 * @brief 将VideoSourceType转换为字符串
 */
inline QString videoSourceTypeToString(VisionInspectionConfig::VideoSourceType type) {
    switch (type) {
        case VisionInspectionConfig::VideoSourceType::None: return "无";
        case VisionInspectionConfig::VideoSourceType::Camera: return "相机";
        case VisionInspectionConfig::VideoSourceType::VideoFile: return "视频文件";
        case VisionInspectionConfig::VideoSourceType::ImageDir: return "图像目录";
        default: return "未知";
    }
}

/**
 * @brief 将字符串转换为VideoSourceType
 */
inline VisionInspectionConfig::VideoSourceType stringToVideoSourceType(const QString& str) {
    if (str == "无") return VisionInspectionConfig::VideoSourceType::None;
    if (str == "相机") return VisionInspectionConfig::VideoSourceType::Camera;
    if (str == "视频文件") return VisionInspectionConfig::VideoSourceType::VideoFile;
    if (str == "图像目录") return VisionInspectionConfig::VideoSourceType::ImageDir;
    return VisionInspectionConfig::VideoSourceType::None;
}