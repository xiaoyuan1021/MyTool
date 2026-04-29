#pragma once

#include <QJsonObject>
#include <opencv2/core.hpp>
#include "core/pipeline_types.h"
#include "config/shape_filter_types.h"

/**
 * @brief Pipeline配置结构
 * 
 * 包含图像增强、颜色过滤、形状筛选、直线检测、条码识别等参数。
 * 从 pipeline.h 中提取，用于降低依赖耦合。
 */
struct PipelineConfig
{
    // 使用独立定义的枚举类型（向后兼容的别名）
    using Channel = ChannelMode;
    using ColorFilterMode = ::ColorFilterMode;
    using FilterMode = ::ImageFilterMode;  // 图像过滤模式

    ChannelMode channel = ChannelMode::RGB;
    ImageFilterMode currentFilterMode = ImageFilterMode::None;

    bool enableColorFilter = false;
    ::ColorFilterMode colorFilterMode = ::ColorFilterMode::None;

    int brightness = 0;
    double contrast = 1.0;
    double gamma = 1.0;
    double sharpen=0.0;

    // 灰度过滤（选中范围）
    int grayLow = 0;
    int grayHigh = 255;
    bool enableGrayFilter = true;
    bool enableAreaFilter =false;


    // RGB 过滤范围
    int rLow = 0, rHigh = 255;
    int gLow = 0, gHigh = 255;
    int bLow = 0, bHigh = 255;

    // HSV 过滤范围
    int hLow = 0, hHigh = 179;
    int sLow = 0, sHigh = 255;
    int vLow = 0, vHigh = 255;

    // 形状筛选配置（替换原来的 minArea/maxArea）
    ShapeFilterConfig shapeFilter;

    // ========== 直线检测参数 ==========
    int lineDetectAlgorithm = 0;  // 0=HoughP, 1=LSD, 2=EDline
    double lineRho = 1.0;
    double lineTheta = CV_PI / 180.0;
    int lineThreshold = 50;
    double lineMinLength = 30.0;
    double lineMaxGap = 10.0;
    bool enableLineDetect = false;

    // ========== 参考线匹配参数 ==========
    bool enableReferenceLineMatch = false;
    cv::Point2f referenceLineStart;
    cv::Point2f referenceLineEnd;
    bool referenceLineValid = false;
    double angleThreshold = 15.0;      // 角度容差（度）
    double distanceThreshold = 50.0;   // 距离容差（像素）
    int searchRegionWidth = 100;       // 搜索区域宽度（像素）
    
     // ========== 条码识别参数 ==========
     BarcodeConfig barcode;

     // ==================== JSON 序列化 ====================

     /**
      * @brief 将 PipelineConfig 序列化为 JSON
      *        集中管理所有序列化字段，避免散落在各调用方
      */
     QJsonObject toJson() const {
         QJsonObject obj;
         obj["brightness"] = brightness;
         obj["contrast"] = contrast;
         obj["gamma"] = gamma;
         obj["sharpen"] = sharpen;
         obj["channel"] = static_cast<int>(channel);

         // 条码参数
         QJsonObject barcodeObj;
         barcodeObj["enableBarcode"] = barcode.enableBarcode;
         barcodeObj["codeTypes"] = barcode.codeTypes.join(",");
         barcodeObj["maxNumSymbols"] = barcode.maxNumSymbols;
         barcodeObj["returnQuality"] = barcode.returnQuality;
         obj["barcode"] = barcodeObj;

         return obj;
     }

     /**
      * @brief 从 JSON 反序列化 PipelineConfig
      *        忽略 JSON 中不存在的字段，保持当前值
      */
     void fromJson(const QJsonObject& obj) {
         if (obj.isEmpty()) return;
         brightness = obj["brightness"].toInt(0);
         contrast   = obj["contrast"].toDouble(1.0);
         gamma      = obj["gamma"].toDouble(1.0);
         sharpen    = obj["sharpen"].toDouble(0.0);
         channel    = static_cast<ChannelMode>(obj["channel"].toInt(0));

         // 条码参数（兼容旧格式：字段名 "barcode" 内嵌对象）
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
 };