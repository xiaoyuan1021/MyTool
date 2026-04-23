#pragma once

#include <QString>
#include <QStringList>
#include <QVariantMap>
#include <QJsonObject>
#include <QJsonArray>

/**
 * @brief 检测配置类型定义
 * 
 * 为不同类型的检测项定义专用的配置结构体
 */

/**
 * @brief Blob分析配置参数
 */
struct BlobAnalysisConfig {
    // 图像增强参数
    int brightness = 0;          // 亮度 (-100 ~ 100)
    int contrast = 100;          // 对比度 (0 ~ 200)
    int gamma = 100;             // 伽马值 (10 ~ 300)
    int sharpen = 100;           // 锐化 (0 ~ 200)
    
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
    
    /**
     * @brief 转换为JSON对象
     */
    QJsonObject toJson() const {
        QJsonObject obj;
        obj["brightness"] = brightness;
        obj["contrast"] = contrast;
        obj["gamma"] = gamma;
        obj["sharpen"] = sharpen;
        obj["minArea"] = minArea;
        obj["maxArea"] = maxArea;
        obj["minCircularity"] = minCircularity;
        obj["maxCircularity"] = maxCircularity;
        obj["minConvexity"] = minConvexity;
        obj["maxConvexity"] = maxConvexity;
        obj["minInertia"] = minInertia;
        obj["maxInertia"] = maxInertia;
        obj["enableCorrection"] = enableCorrection;
        obj["correctionMethod"] = correctionMethod;
        obj["correctionThreshold"] = correctionThreshold;
        obj["thresholdType"] = thresholdType;
        obj["thresholdValue"] = thresholdValue;
        obj["invertBinary"] = invertBinary;
        obj["morphologyOp"] = morphologyOp;
        obj["morphologySize"] = morphologySize;
        obj["extractionMethod"] = extractionMethod;
        obj["useHierarchy"] = useHierarchy;
        obj["contourMode"] = contourMode;
        obj["minBlobCount"] = minBlobCount;
        obj["maxBlobCount"] = maxBlobCount;
        obj["minAreaThreshold"] = minAreaThreshold;
        obj["maxAreaThreshold"] = maxAreaThreshold;
        return obj;
    }
    
    /**
     * @brief 从JSON对象加载
     */
    void fromJson(const QJsonObject& obj) {
        brightness = obj["brightness"].toInt(0);
        contrast = obj["contrast"].toInt(100);
        gamma = obj["gamma"].toInt(100);
        sharpen = obj["sharpen"].toInt(100);
        minArea = obj["minArea"].toInt(100);
        maxArea = obj["maxArea"].toInt(10000);
        minCircularity = obj["minCircularity"].toDouble(0.5);
        maxCircularity = obj["maxCircularity"].toDouble(1.0);
        minConvexity = obj["minConvexity"].toDouble(0.8);
        maxConvexity = obj["maxConvexity"].toDouble(1.0);
        minInertia = obj["minInertia"].toDouble(0.5);
        maxInertia = obj["maxInertia"].toDouble(1.0);
        enableCorrection = obj["enableCorrection"].toBool(false);
        correctionMethod = obj["correctionMethod"].toInt(0);
        correctionThreshold = obj["correctionThreshold"].toDouble(0.8);
        thresholdType = obj["thresholdType"].toInt(0);
        thresholdValue = obj["thresholdValue"].toInt(128);
        invertBinary = obj["invertBinary"].toBool(false);
        morphologyOp = obj["morphologyOp"].toInt(0);
        morphologySize = obj["morphologySize"].toInt(3);
        extractionMethod = obj["extractionMethod"].toInt(0);
        useHierarchy = obj["useHierarchy"].toBool(true);
        contourMode = obj["contourMode"].toInt(0);
        minBlobCount = obj["minBlobCount"].toInt(0);
        maxBlobCount = obj["maxBlobCount"].toInt(100);
        minAreaThreshold = obj["minAreaThreshold"].toDouble(0);
        maxAreaThreshold = obj["maxAreaThreshold"].toDouble(1000000);
    }
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
    
    // 图像处理参数
    int brightness = 0;                // 亮度 (-100 ~ 100)
    int contrast = 100;                // 对比度 (0 ~ 200)
    int gamma = 100;                   // 伽马值 (10 ~ 300)
    int sharpen = 100;                 // 锐化 (0 ~ 200)
    
    // 帧处理参数
    bool enableFrameSkipping = false;  // 是否启用帧跳跃
    int frameSkipInterval = 1;         // 帧跳跃间隔
    bool enableFrameBlur = false;      // 是否启用去噪
    int blurKernelSize = 3;            // 模糊核大小
    
    /**
     * @brief 转换为JSON对象
     */
    QJsonObject toJson() const {
        QJsonObject obj;
        obj["videoSourceType"] = videoSourceType;
        obj["videoFilePath"] = videoFilePath;
        obj["cameraIndex"] = cameraIndex;
        obj["autoPlay"] = autoPlay;
        obj["playbackSpeed"] = playbackSpeed;
        obj["brightness"] = brightness;
        obj["contrast"] = contrast;
        obj["gamma"] = gamma;
        obj["sharpen"] = sharpen;
        obj["enableFrameSkipping"] = enableFrameSkipping;
        obj["frameSkipInterval"] = frameSkipInterval;
        obj["enableFrameBlur"] = enableFrameBlur;
        obj["blurKernelSize"] = blurKernelSize;
        return obj;
    }
    
    /**
     * @brief 从JSON对象加载
     */
    void fromJson(const QJsonObject& obj) {
        videoSourceType = obj["videoSourceType"].toString("file");
        videoFilePath = obj["videoFilePath"].toString();
        cameraIndex = obj["cameraIndex"].toInt(0);
        autoPlay = obj["autoPlay"].toBool(true);
        playbackSpeed = obj["playbackSpeed"].toDouble(1.0);
        brightness = obj["brightness"].toInt(0);
        contrast = obj["contrast"].toInt(100);
        gamma = obj["gamma"].toInt(100);
        sharpen = obj["sharpen"].toInt(100);
        enableFrameSkipping = obj["enableFrameSkipping"].toBool(false);
        frameSkipInterval = obj["frameSkipInterval"].toInt(1);
        enableFrameBlur = obj["enableFrameBlur"].toBool(false);
        blurKernelSize = obj["blurKernelSize"].toInt(3);
    }
};

/**
 * @brief 直线检测配置参数
 */
struct LineDetectionConfig {
    // 图像增强参数
    int brightness = 0;          // 亮度
    int contrast = 100;          // 对比度
    int gamma = 100;             // 伽马值
    int sharpen = 100;           // 锐化
    
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
    
    /**
     * @brief 转换为JSON对象
     */
    QJsonObject toJson() const {
        QJsonObject obj;
        obj["brightness"] = brightness;
        obj["contrast"] = contrast;
        obj["gamma"] = gamma;
        obj["sharpen"] = sharpen;
        obj["algorithm"] = algorithm;
        obj["rho"] = rho;
        obj["theta"] = theta;
        obj["threshold"] = threshold;
        obj["minLineLength"] = minLineLength;
        obj["maxLineGap"] = maxLineGap;
        obj["enableReferenceLine"] = enableReferenceLine;
        obj["angleThreshold"] = angleThreshold;
        obj["distanceThreshold"] = distanceThreshold;
        obj["searchRegionWidth"] = searchRegionWidth;
        return obj;
    }
    
    /**
     * @brief 从JSON对象加载
     */
    void fromJson(const QJsonObject& obj) {
        brightness = obj["brightness"].toInt(0);
        contrast = obj["contrast"].toInt(100);
        gamma = obj["gamma"].toInt(100);
        sharpen = obj["sharpen"].toInt(100);
        algorithm = obj["algorithm"].toInt(0);
        rho = obj["rho"].toDouble(1.0);
        theta = obj["theta"].toDouble(3.14159265358979323846 / 180.0);
        threshold = obj["threshold"].toInt(50);
        minLineLength = obj["minLineLength"].toDouble(30.0);
        maxLineGap = obj["maxLineGap"].toDouble(10.0);
        enableReferenceLine = obj["enableReferenceLine"].toBool(false);
        angleThreshold = obj["angleThreshold"].toDouble(10.0);
        distanceThreshold = obj["distanceThreshold"].toDouble(20.0);
        searchRegionWidth = obj["searchRegionWidth"].toInt(50);
    }
};

/**
 * @brief 条码识别配置参数
 */
struct BarcodeRecognitionConfig {
    // 图像增强参数
    int brightness = 0;          // 亮度
    int contrast = 100;          // 对比度
    int gamma = 100;             // 伽马值
    int sharpen = 100;           // 锐化
    
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
    
    /**
     * @brief 转换为JSON对象
     */
    QJsonObject toJson() const {
        QJsonObject obj;
        obj["brightness"] = brightness;
        obj["contrast"] = contrast;
        obj["gamma"] = gamma;
        obj["sharpen"] = sharpen;
        obj["enableBarcode"] = enableBarcode;
        QJsonArray codeTypesArray;
        for (const QString& type : codeTypes) {
            codeTypesArray.append(type);
        }
        obj["codeTypes"] = codeTypesArray;
        obj["maxNumSymbols"] = maxNumSymbols;
        obj["returnQuality"] = returnQuality;
        obj["minConfidence"] = minConfidence;
        obj["timeout"] = timeout;
        obj["enablePreprocessing"] = enablePreprocessing;
        obj["preprocessMethod"] = preprocessMethod;
        obj["binarizationThreshold"] = binarizationThreshold;
        obj["morphologySize"] = morphologySize;
        return obj;
    }
    
    /**
     * @brief 从JSON对象加载
     */
    void fromJson(const QJsonObject& obj) {
        brightness = obj["brightness"].toInt(0);
        contrast = obj["contrast"].toInt(100);
        gamma = obj["gamma"].toInt(100);
        sharpen = obj["sharpen"].toInt(100);
        enableBarcode = obj["enableBarcode"].toBool(true);
        codeTypes.clear();
        QJsonArray codeTypesArray = obj["codeTypes"].toArray();
        for (const QJsonValue& val : codeTypesArray) {
            codeTypes.append(val.toString());
        }
        if (codeTypes.isEmpty()) {
            codeTypes = {"auto"};
        }
        maxNumSymbols = obj["maxNumSymbols"].toInt(0);
        returnQuality = obj["returnQuality"].toBool(true);
        minConfidence = obj["minConfidence"].toDouble(0.7);
        timeout = obj["timeout"].toInt(5000);
        enablePreprocessing = obj["enablePreprocessing"].toBool(true);
        preprocessMethod = obj["preprocessMethod"].toInt(0);
        binarizationThreshold = obj["binarizationThreshold"].toInt(128);
        morphologySize = obj["morphologySize"].toInt(3);
    }
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
    
    /**
     * @brief 转换为JSON对象
     */
    QJsonObject toJson() const {
        QJsonObject obj;
        obj["modelPath"] = modelPath;
        obj["configPath"] = configPath;
        obj["confidenceThreshold"] = static_cast<double>(confidenceThreshold);
        obj["nmsThreshold"] = static_cast<double>(nmsThreshold);
        obj["inputWidth"] = inputWidth;
        obj["inputHeight"] = inputHeight;
        obj["showLabels"] = showLabels;
        obj["showConfidence"] = showConfidence;
        obj["showBoundingBox"] = showBoundingBox;
        obj["lineWidth"] = lineWidth;
        return obj;
    }
    
    /**
     * @brief 从JSON对象加载
     */
    void fromJson(const QJsonObject& obj) {
        modelPath = obj["modelPath"].toString();
        configPath = obj["configPath"].toString();
        confidenceThreshold = static_cast<float>(obj["confidenceThreshold"].toDouble(0.5));
        nmsThreshold = static_cast<float>(obj["nmsThreshold"].toDouble(0.4));
        inputWidth = obj["inputWidth"].toInt(640);
        inputHeight = obj["inputHeight"].toInt(640);
        showLabels = obj["showLabels"].toBool(true);
        showConfidence = obj["showConfidence"].toBool(true);
        showBoundingBox = obj["showBoundingBox"].toBool(true);
        lineWidth = obj["lineWidth"].toInt(2);
    }
};
