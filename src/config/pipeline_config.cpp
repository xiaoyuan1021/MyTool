#include "config/pipeline_config.h"

QJsonObject PipelineConfig::toJson() const {
    QJsonObject obj;

    // 增强参数（现在都是 int 原始值）
    obj["enhance"] = enhance.toJson();

    // 通道/过滤参数
    QJsonObject colorObj;
    colorObj["channel"] = static_cast<int>(colorFilter.channel);
    colorObj["currentFilterMode"] = static_cast<int>(colorFilter.currentFilterMode);
    colorObj["enableColorFilter"] = colorFilter.enableColorFilter;
    colorObj["colorFilterMode"] = static_cast<int>(colorFilter.colorFilterMode);
    colorObj["grayLow"] = colorFilter.grayLow;
    colorObj["grayHigh"] = colorFilter.grayHigh;
    colorObj["enableGrayFilter"] = colorFilter.enableGrayFilter;
    colorObj["enableAreaFilter"] = colorFilter.enableAreaFilter;
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

    return obj;
}

void PipelineConfig::fromJson(const QJsonObject& obj) {
    if (obj.isEmpty()) return;

    // 增强参数
    if (obj.contains("enhance")) {
        enhance.fromJson(obj["enhance"].toObject());
    } else {
        // 向后兼容：旧格式直接平铺在顶层
        enhance.brightness = obj["brightness"].toInt(0);
        enhance.contrast   = obj["contrast"].toInt(100);
        enhance.gamma      = obj["gamma"].toInt(100);
        enhance.sharpen    = obj["sharpen"].toInt(100);
    }

    // 通道/过滤参数
    if (obj.contains("colorFilter")) {
        QJsonObject colorObj = obj["colorFilter"].toObject();
        colorFilter.channel          = static_cast<ChannelMode>(colorObj["channel"].toInt(0));
        colorFilter.currentFilterMode = static_cast<ImageFilterMode>(colorObj["currentFilterMode"].toInt(0));
        colorFilter.enableColorFilter = colorObj["enableColorFilter"].toBool(false);
        colorFilter.colorFilterMode  = static_cast<::ColorFilterMode>(colorObj["colorFilterMode"].toInt(0));
        colorFilter.grayLow          = colorObj["grayLow"].toInt(0);
        colorFilter.grayHigh         = colorObj["grayHigh"].toInt(255);
        colorFilter.enableGrayFilter = colorObj["enableGrayFilter"].toBool(true);
        colorFilter.enableAreaFilter = colorObj["enableAreaFilter"].toBool(false);
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
    } else {
        // 向后兼容：旧格式平铺在顶层
        colorFilter.channel = static_cast<ChannelMode>(obj["channel"].toInt(0));
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
        barcode.enableBarcode = barcodeObj["enableBarcode"].toBool(false);
        QString codeTypesStr  = barcodeObj["codeTypes"].toString();
        if (!codeTypesStr.isEmpty()) {
            barcode.codeTypes = codeTypesStr.split(",");
        }
        barcode.maxNumSymbols = barcodeObj["maxNumSymbols"].toInt(0);
        barcode.returnQuality = barcodeObj["returnQuality"].toBool(true);
    }
}