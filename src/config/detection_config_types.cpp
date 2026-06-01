#include "config/detection_config_types.h"

// ========== BlobAnalysisConfig ==========

QJsonObject BlobAnalysisConfig::toJson() const {
    QJsonObject obj;
    obj["brightness"] = enhance.brightness;
    obj["contrast"] = enhance.contrast;
    obj["gamma"] = enhance.gamma;
    obj["sharpen"] = enhance.sharpen;
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

void BlobAnalysisConfig::fromJson(const QJsonObject& obj) {
    enhance.brightness = obj["brightness"].toInt(0);
    enhance.contrast = obj["contrast"].toInt(100);
    enhance.gamma = obj["gamma"].toInt(100);
    enhance.sharpen = obj["sharpen"].toInt(100);
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

// ========== VideoSourceConfig ==========

QJsonObject VideoSourceConfig::toJson() const {
    QJsonObject obj;
    obj["videoSourceType"] = videoSourceType;
    obj["videoFilePath"] = videoFilePath;
    obj["cameraIndex"] = cameraIndex;
    obj["autoPlay"] = autoPlay;
    obj["playbackSpeed"] = playbackSpeed;
    obj["brightness"] = enhance.brightness;
    obj["contrast"] = enhance.contrast;
    obj["gamma"] = enhance.gamma;
    obj["sharpen"] = enhance.sharpen;
    obj["enableFrameSkipping"] = enableFrameSkipping;
    obj["frameSkipInterval"] = frameSkipInterval;
    obj["enableFrameBlur"] = enableFrameBlur;
    obj["blurKernelSize"] = blurKernelSize;
    return obj;
}

void VideoSourceConfig::fromJson(const QJsonObject& obj) {
    videoSourceType = obj["videoSourceType"].toString("file");
    videoFilePath = obj["videoFilePath"].toString();
    cameraIndex = obj["cameraIndex"].toInt(0);
    autoPlay = obj["autoPlay"].toBool(true);
    playbackSpeed = obj["playbackSpeed"].toDouble(1.0);
    enhance.brightness = obj["brightness"].toInt(0);
    enhance.contrast = obj["contrast"].toInt(100);
    enhance.gamma = obj["gamma"].toInt(100);
    enhance.sharpen = obj["sharpen"].toInt(100);
    enableFrameSkipping = obj["enableFrameSkipping"].toBool(false);
    frameSkipInterval = obj["frameSkipInterval"].toInt(1);
    enableFrameBlur = obj["enableFrameBlur"].toBool(false);
    blurKernelSize = obj["blurKernelSize"].toInt(3);
}

// 注意：LineDetectionConfig 和 BarcodeRecognitionConfig 的序列化
// 已移至 pipeline_config.cpp（LineDetectConfig::toJson/fromJson 和 BarcodeConfig::toJson/fromJson）

// 注意：ObjectDetectionConfig 的序列化已移至 pipeline_config.h（内联实现）

// ========== VideoDetectionConfig ==========

QJsonObject VideoDetectionConfig::toJson() const {
    QJsonObject obj;
    obj["videoSource"] = videoSource;
    obj["cameraIndex"] = cameraIndex;
    obj["videoFilePath"] = videoFilePath;
    obj["autoPlay"] = autoPlay;
    obj["playbackSpeed"] = playbackSpeed;
    obj["loopPlayback"] = loopPlayback;
    obj["modelPath"] = modelPath;
    obj["configPath"] = configPath;
    obj["confidenceThreshold"] = static_cast<double>(confidenceThreshold);
    obj["nmsThreshold"] = static_cast<double>(nmsThreshold);
    obj["inputWidth"] = inputWidth;
    obj["inputHeight"] = inputHeight;
    return obj;
}

void VideoDetectionConfig::fromJson(const QJsonObject& obj) {
    videoSource = obj["videoSource"].toString("camera");
    cameraIndex = obj["cameraIndex"].toInt(0);
    videoFilePath = obj["videoFilePath"].toString();
    autoPlay = obj["autoPlay"].toBool(true);
    playbackSpeed = obj["playbackSpeed"].toDouble(1.0);
    loopPlayback = obj["loopPlayback"].toBool(true);
    modelPath = obj["modelPath"].toString();
    configPath = obj["configPath"].toString();
    confidenceThreshold = static_cast<float>(obj["confidenceThreshold"].toDouble(0.5));
    nmsThreshold = static_cast<float>(obj["nmsThreshold"].toDouble(0.4));
    inputWidth = obj["inputWidth"].toInt(640);
    inputHeight = obj["inputHeight"].toInt(640);
}

// ========== OcrDetectionConfig ==========

QJsonObject OcrDetectionConfig::toJson() const {
    QJsonObject obj;
    // 增强参数扁平展开（兼容旧格式）
    obj["brightness"] = enhance.brightness;
    obj["contrast"] = enhance.contrast;
    obj["gamma"] = enhance.gamma;
    obj["sharpen"] = enhance.sharpen;
    // OCR核心参数通过 OcrConfig 序列化
    QJsonObject ocrObj = ocr.toJson();
    for (auto it = ocrObj.begin(); it != ocrObj.end(); ++it) {
        obj[it.key()] = it.value();
    }
    // 判定参数
    obj["enableTextFilter"] = enableTextFilter;
    obj["expectedText"] = expectedText;
    obj["matchExact"] = matchExact;
    return obj;
}

void OcrDetectionConfig::fromJson(const QJsonObject& obj) {
    enhance.brightness = obj["brightness"].toInt(0);
    enhance.contrast = obj["contrast"].toInt(100);
    enhance.gamma = obj["gamma"].toInt(100);
    enhance.sharpen = obj["sharpen"].toInt(100);
    // OCR核心参数通过 OcrConfig 反序列化
    QJsonObject ocrObj;
    ocrObj["language"] = obj["language"];
    ocrObj["pageMode"] = obj["pageMode"];
    ocrObj["dpi"] = obj["dpi"];
    ocrObj["confidenceThreshold"] = obj["confidenceThreshold"];
    ocrObj["whitelist"] = obj["whitelist"];
    ocr.fromJson(ocrObj);
    // 判定参数
    enableTextFilter = obj["enableTextFilter"].toBool(false);
    expectedText = obj["expectedText"].toString();
    matchExact = obj["matchExact"].toBool(false);
}