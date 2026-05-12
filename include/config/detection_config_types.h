#pragma once

#include <QString>
#include <QStringList>
#include <QVariantMap>
#include <QJsonObject>
#include <QJsonArray>
#include "config/json_serializable.h"
#include "config/pipeline_config.h"

/**
 * @brief 检测项（DetectionItem）专用配置类型定义
 * 
 * ⚠️ 注意：这些配置类型与 pipeline_config.h 中的类型不同！
 * 
 * - 本文件的类型：用于 DetectionItem 的参数存储（保存在 RoiConfig.detectionItems 中），
 *   每个检测项独立持有自己的增强、过滤、检测参数。
 * 
 * - pipeline_config.h 的类型：用于 Pipeline 执行时的全局配置（PipelineConfig），
 *   由 PipelineManager 持有，在 Pipeline 执行时传入。
 * 
 * 两套配置系统并存的原因：
 *   DetectionItem 支持一个 ROI 下挂多个不同类型的检测（如同时做 Blob + 条码），
 *   每个检测项需要独立的参数；而 PipelineConfig 是当前活跃的全局流水线参数。
 */

/**
 * @brief Blob分析配置参数
 */
struct BlobAnalysisConfig : public IJsonSerializable {
    // 图像增强参数（统一类型定义，独立实例）
    EnhanceConfig enhance;
    
    // 过滤参数
    int minArea = 100;           // 最小面积
    int maxArea = 10000;         // 最大面积
    double minCircularity = 0.5; // 最小圆形度 (0.0 ~ 1.0)
    double maxCircularity = 1.0; // 最大圆形度 (0.0 ~ 1.0)
    double minConvexity = 0.8;   // 最小凸性 (0.0 ~ 1.0)
    double maxConvexity = 1.0;   // 最大凸性 (0.0 ~ 1.0)
    double minInertia = 0.5;     // 最小惯性比 (0.0 ~ 1.0)
    double maxInertia = 1.0;     // 最大惯性比 (0.0 ~ 1.0)
    
    // 补正参数
    bool enableCorrection = false;      // 是否启用补正
    int correctionMethod = 0;           // 补正方法 (0: 无, 1: 平移, 2: 旋转, 3: 仿射)
    double correctionThreshold = 0.8;   // 补正阈值
    
    // 处理参数
    int thresholdType = 0;       // 阈值类型 (0: 二进制, 1: 反二进制, 2: 截断, 3: 大津法)
    int thresholdValue = 128;    // 阈值
    bool invertBinary = false;   // 是否反转二值图像
    int morphologyOp = 0;        // 形态学操作 (0: 无, 1: 腐蚀, 2: 膨胀, 3: 开运算, 4: 闭运算)
    int morphologySize = 3;      // 形态学核大小
    
    // 提取参数
    int extractionMethod = 0;    // 提取方法 (0: 轮廓, 1: 连通域, 2: 特征点)
    bool useHierarchy = true;    // 是否使用轮廓层次结构
    int contourMode = 0;         // 轮廓模式 (0: 外轮廓, 1: 所有轮廓, 2: 简单轮廓)
    
    // 判定参数
    int minBlobCount = 0;        // 最小Blob数量
    int maxBlobCount = 100;      // 最大Blob数量
    double minAreaThreshold = 0; // 最小面积阈值
    double maxAreaThreshold = 1000000; // 最大面积阈值
    
    // JSON 序列化（定义在 detection_config_types.cpp）
    QJsonObject toJson() const;
    void fromJson(const QJsonObject& obj);
};

/**
 * @brief 视频源配置参数
 */
struct VideoSourceConfig {
    // 视频源参数
    QString videoSourceType = "file";  // 视频源类型: "file" (文件) 或 "camera" (相机)
    QString videoFilePath = "";        // 视频文件路径
    int cameraIndex = 0;               // 相机索引
    
    // 播放控制参数
    bool autoPlay = true;              // 自动播放
    double playbackSpeed = 1.0;        // 播放速度 (0.5 ~ 2.0)
    
    // 图像处理参数（统一类型定义，独立实例）
    EnhanceConfig enhance;
    
    // 帧处理参数
    bool enableFrameSkipping = false;  // 是否启用帧跳跃
    int frameSkipInterval = 1;         // 帧跳跃间隔
    bool enableFrameBlur = false;      // 是否启用去噪
    int blurKernelSize = 3;            // 模糊核大小
    
    // JSON 序列化（定义在 detection_config_types.cpp）
    QJsonObject toJson() const;
    void fromJson(const QJsonObject& obj);
};

/**
 * @brief 直线检测配置参数
 */
struct LineDetectionConfig {
    // 图像增强参数（统一类型定义，独立实例）
    EnhanceConfig enhance;
    
    // 直线检测参数
    int algorithm = 0;           // 算法 (0: HoughP, 1: LSD, 2: EDline)
    double rho = 1.0;            // 距离分辨率
    double theta = 3.14159265358979323846 / 180.0; // 角度分辨率
    int threshold = 50;          // 阈值
    double minLineLength = 30.0; // 最小线段长度
    double maxLineGap = 10.0;    // 最大线段间隙
    
    // 参考线匹配参数
    bool enableReferenceLine = false;   // 是否启用参考线匹配
    double angleThreshold = 10.0;       // 角度阈值（度）
    double distanceThreshold = 20.0;    // 距离阈值（像素）
    int searchRegionWidth = 50;         // 搜索区域宽度（像素）
    
    // JSON 序列化（定义在 detection_config_types.cpp）
    QJsonObject toJson() const;
    void fromJson(const QJsonObject& obj);
};

/**
 * @brief 条码识别配置参数
 */
struct BarcodeRecognitionConfig {
    // 图像增强参数（统一类型定义，独立实例）
    EnhanceConfig enhance;
    
    // 条码识别参数
    bool enableBarcode = true;           // 是否启用条码识别
    QStringList codeTypes = {"auto"};    // 条码类型
    int maxNumSymbols = 0;               // 最大识别数量 (0=不限制)
    bool returnQuality = true;           // 是否返回质量信息
    double minConfidence = 0.7;          // 最小置信度
    int timeout = 5000;                  // 超时时间（毫秒）
    
    // 图像预处理参数
    bool enablePreprocessing = true;     // 是否启用预处理
    int preprocessMethod = 0;            // 预处理方法 (0: 直接识别, 1: 二值化, 2: 形态学)
    int binarizationThreshold = 128;     // 二值化阈值
    int morphologySize = 3;              // 形态学核大小
    
    // JSON 序列化（定义在 detection_config_types.cpp）
    QJsonObject toJson() const;
    void fromJson(const QJsonObject& obj);
};

/**
 * @brief 目标检测配置参数
 */
struct ObjectDetectionConfig {
    // 模型参数
    QString modelPath = "";                 // 模型文件路径（支持 ONNX/Caffe/TF/Darknet）
    QString configPath = "";                // 配置文件路径（可选）
    
    // 检测参数
    float confidenceThreshold = 0.5f;       // 置信度阈值 (0.0 ~ 1.0)
    float nmsThreshold = 0.4f;              // 非极大值抑制阈值 (0.0 ~ 1.0)
    int inputWidth = 640;                    // 输入宽度
    int inputHeight = 640;                   // 输入高度
    
    // 显示参数
    bool showLabels = true;                  // 是否显示标签
    bool showConfidence = true;              // 是否显示置信度
    bool showBoundingBox = true;             // 是否显示边框
    int lineWidth = 2;                       // 边框线宽
    
    // JSON 序列化（定义在 detection_config_types.cpp）
    QJsonObject toJson() const;
    void fromJson(const QJsonObject& obj);
};

/**
 * @brief 视频检测配置参数（视频源 + 目标检测）
 */
struct VideoDetectionConfig {
    // 视频源参数
    QString videoSource;           // 视频源类型: "camera", "file"
    int cameraIndex = 0;           // 相机索引
    QString videoFilePath = "";    // 视频文件路径
    
    // 播放控制
    bool autoPlay = true;          // 自动播放
    double playbackSpeed = 1.0;    // 播放速度
    bool loopPlayback = true;      // 循环播放
    
    // 检测参数
    QString modelPath = "";        // 模型文件路径
    QString configPath = "";       // 配置文件路径
    float confidenceThreshold = 0.5f;
    float nmsThreshold = 0.4f;
    int inputWidth = 640;
    int inputHeight = 640;
    
    // JSON 序列化（定义在 detection_config_types.cpp）
    QJsonObject toJson() const;
    void fromJson(const QJsonObject& obj);
};
