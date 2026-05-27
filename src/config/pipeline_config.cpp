#include "config/pipeline_config.h"
#include <QJsonArray>

QJsonObject PipelineConfig::toJson() const {
    QJsonObject obj;

    // 增强参数（现在都是 int 原始值）
    obj["enhance"] = enhance.toJson();

    // 通道/过滤参数
    QJsonObject colorObj;
    colorObj["channel"] = static_cast<int>(colorFilter.channel);
    colorObj["filterMode"] = static_cast<int>(colorFilter.mode);
    colorObj["grayLow"] = colorFilter.grayLow;
    colorObj["grayHigh"] = colorFilter.grayHigh;
    colorObj["rLow"] = colorFilter.rLow;
    colorObj["rHigh"] = colorFilter.rHigh;
    colorObj["gLow"] = colorFilter.gLow;
    colorObj["gHigh"] = colorFilter.gHigh;
    colorObj["bLow"] = colorFilter.bLow;
    colorObj["bHigh"] = colorFilter.bHigh;
    colorObj["hLow"] = colorFilter.hLow;
    colorObj["hHigh"] = colorFilter.hHigh;
    colorObj["sLow"] = colorFilter.sLow;
    colorObj["sHigh"] = colorFilter.sHigh;
    colorObj["vLow"] = colorFilter.vLow;
    colorObj["vHigh"] = colorFilter.vHigh;
    obj["colorFilter"] = colorObj;

    // 直线检测参数
    QJsonObject lineObj;
    lineObj["algorithm"] = lineDetect.algorithm;
    lineObj["threshold"] = lineDetect.threshold;
    lineObj["minLength"] = lineDetect.minLength;
    lineObj["maxGap"] = lineDetect.maxGap;
    lineObj["enabled"] = lineDetect.enabled;
    obj["lineDetect"] = lineObj;

    // 条码参数
    QJsonObject barcodeObj;
    barcodeObj["enableBarcode"] = barcode.enableBarcode;
    barcodeObj["codeTypes"] = barcode.codeTypes.join(",");
    barcodeObj["maxNumSymbols"] = barcode.maxNumSymbols;
    barcodeObj["returnQuality"] = barcode.returnQuality;
    obj["barcode"] = barcodeObj;

    // 滤波去噪参数
    obj["imageFilter"] = imageFilter.toJson();

    // OCR参数
    obj["ocr"] = ocr.toJson();

    // 步骤控制
    QJsonArray enabledArr, orderArr;
    for (int i = 0; i < PipelineConfig::STEP_COUNT; ++i) {
        enabledArr.append(stepEnabled[i]);
        orderArr.append(stepOrder[i]);
    }
    obj["stepEnabled"] = enabledArr;
    obj["stepOrder"] = orderArr;
    obj["enableObjectDetection"] = enableObjectDetection;

    return obj;
}

void PipelineConfig::resetToDefaults()
{
    *this = PipelineConfig();
}

void PipelineConfig::fromJson(const QJsonObject& obj) {
    if (obj.isEmpty()) return;

    // 增强参数
    if (obj.contains("enhance")) {
        enhance.fromJson(obj["enhance"].toObject());
    }

    // 通道/过滤参数
    if (obj.contains("colorFilter")) {
        QJsonObject colorObj = obj["colorFilter"].toObject();
        colorFilter.channel = static_cast<ChannelMode>(colorObj["channel"].toInt(0));
        colorFilter.mode = static_cast<ImageFilterMode>(colorObj["filterMode"].toInt(0));
        colorFilter.grayLow = colorObj["grayLow"].toInt(0);
        colorFilter.grayHigh = colorObj["grayHigh"].toInt(255);
        colorFilter.rLow = colorObj["rLow"].toInt(0);
        colorFilter.rHigh = colorObj["rHigh"].toInt(255);
        colorFilter.gLow = colorObj["gLow"].toInt(0);
        colorFilter.gHigh = colorObj["gHigh"].toInt(255);
        colorFilter.bLow = colorObj["bLow"].toInt(0);
        colorFilter.bHigh = colorObj["bHigh"].toInt(255);
        colorFilter.hLow = colorObj["hLow"].toInt(0);
        colorFilter.hHigh = colorObj["hHigh"].toInt(179);
        colorFilter.sLow = colorObj["sLow"].toInt(0);
        colorFilter.sHigh = colorObj["sHigh"].toInt(255);
        colorFilter.vLow = colorObj["vLow"].toInt(0);
        colorFilter.vHigh = colorObj["vHigh"].toInt(255);
    }

    // 直线检测参数
    if (obj.contains("lineDetect")) {
        QJsonObject lineObj = obj["lineDetect"].toObject();
        lineDetect.algorithm   = lineObj["algorithm"].toInt(0);
        lineDetect.threshold   = lineObj["threshold"].toInt(50);
        lineDetect.minLength   = lineObj["minLength"].toDouble(30.0);
        lineDetect.maxGap      = lineObj["maxGap"].toDouble(10.0);
        lineDetect.enabled     = lineObj["enabled"].toBool(false);
    }

    // 条码参数
    QJsonObject barcodeObj = obj["barcode"].toObject();
    if (!barcodeObj.isEmpty()) {
        barcode.fromJson(barcodeObj);
    }

    // 滤波去噪参数
    if (obj.contains("imageFilter")) {
        imageFilter.fromJson(obj["imageFilter"].toObject());
    }

    // OCR参数
    if (obj.contains("ocr")) {
        ocr.fromJson(obj["ocr"].toObject());
    }

    // 步骤控制
    QJsonArray enabledArr = obj["stepEnabled"].toArray();
    if (enabledArr.size() == PipelineConfig::STEP_COUNT) {
        for (int i = 0; i < PipelineConfig::STEP_COUNT; ++i) {
            stepEnabled[i] = enabledArr[i].toBool(true);
        }
    }
    QJsonArray orderArr = obj["stepOrder"].toArray();
    if (orderArr.size() == PipelineConfig::STEP_COUNT) {
        for (int i = 0; i < PipelineConfig::STEP_COUNT; ++i) {
            stepOrder[i] = orderArr[i].toInt(i);
        }
    }

    enableObjectDetection = obj["enableObjectDetection"].toBool(false);
}

// ========== LineDetectConfig 序列化 ==========

QJsonObject LineDetectConfig::toJson() const {
    QJsonObject obj;
    obj["enhance"] = enhance.toJson();
    obj["algorithm"] = algorithm;
    obj["rho"] = rho;
    obj["theta"] = theta;
    obj["threshold"] = threshold;
    obj["minLength"] = minLength;
    obj["maxGap"] = maxGap;
    obj["enabled"] = enabled;
    obj["enableReferenceLineMatch"] = enableReferenceLineMatch;
    obj["angleThreshold"] = angleThreshold;
    obj["distanceThreshold"] = distanceThreshold;
    obj["searchRegionWidth"] = searchRegionWidth;
    return obj;
}

void LineDetectConfig::fromJson(const QJsonObject& obj) {
    if (obj.contains("enhance")) {
        enhance.fromJson(obj["enhance"].toObject());
    }
    algorithm = obj["algorithm"].toInt(0);
    rho = obj["rho"].toDouble(1.0);
    theta = obj["theta"].toDouble(CV_PI / 180.0);
    threshold = obj["threshold"].toInt(50);
    minLength = obj["minLength"].toDouble(30.0);
    maxGap = obj["maxGap"].toDouble(10.0);
    enabled = obj["enabled"].toBool(false);
    enableReferenceLineMatch = obj["enableReferenceLineMatch"].toBool(false);
    angleThreshold = obj["angleThreshold"].toDouble(10.0);
    distanceThreshold = obj["distanceThreshold"].toDouble(20.0);
    searchRegionWidth = obj["searchRegionWidth"].toInt(50);
}

// ========== BarcodeConfig 序列化 ==========

QJsonObject BarcodeConfig::toJson() const {
    QJsonObject obj;
    obj["enhance"] = enhance.toJson();
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

void BarcodeConfig::fromJson(const QJsonObject& obj) {
    if (obj.contains("enhance")) {
        enhance.fromJson(obj["enhance"].toObject());
    }
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
