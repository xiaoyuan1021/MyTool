#include "config/detection_config_types.h"

// ========== BlobAnalysisConfig ==========

QJsonObject BlobAnalysisConfig::toJson() const {
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

void BlobAnalysisConfig::fromJson(const QJsonObject& obj) {
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

// ========== VideoSourceConfig ==========

QJsonObject VideoSourceConfig::toJson() const {
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

void VideoSourceConfig::fromJson(const QJsonObject& obj) {
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

// ========== LineDetectionConfig ==========

QJsonObject LineDetectionConfig::toJson() const {
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

void LineDetectionConfig::fromJson(const QJsonObject& obj) {
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

// ========== BarcodeRecognitionConfig ==========

QJsonObject BarcodeRecognitionConfig::toJson() const {
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

void BarcodeRecognitionConfig::fromJson(const QJsonObject& obj) {
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

// ========== ObjectDetectionConfig ==========

QJsonObject ObjectDetectionConfig::toJson() const {
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

void ObjectDetectionConfig::fromJson(const QJsonObject& obj) {
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