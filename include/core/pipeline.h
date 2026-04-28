#pragma once
#include <opencv2/opencv.hpp>
#include <QVector>
#include <QVariantMap>
#include <QJsonObject>
#include <QPointF>
#include <QString>
#include <QRectF>
#include "shape_filter_types.h"
#include "pipeline_types.h"
#include "region_feature.h"
#include "display_config.h"
#include "algorithm/opencv_algorithm.h"

/**
 * 条码识别结果
 */
struct BarcodeResult
{
    QString type;           // 条码类型 (如 "EAN-13", "QR Code")
    QString data;           // 解码数据
    QRectF location;        // 位置 (bounding box)
    double quality = 0.0;   // 识别质量 (0-100)
    int orientation = 0;    // 条码方向角度
};

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

    // ✅ 形状筛选配置（替换原来的 minArea/maxArea）
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

struct PipelineContext
{
    DisplayConfig displayConfig;
    // 输入
    cv::Mat srcBgr;      // 原图(3通道)
    // 中间结果
    cv::Mat channelImg;  // 颜色通道输出（一般是灰度）
    cv::Mat enhanced;    // 增强后
    cv::Mat mask;        // 过滤得到的 mask (0/255)
    cv::Mat processed;   // 算法处理后的图（可能还是 mask / 或灰度）
    cv::Mat lineDetect;  // 直线检测结果（新增）
    // 特征
    std::vector<RegionFeature> regions;
    // 输出

    int currentRegions=0;

    bool pass = true;
    QString reason;
    
    // 参考线匹配结果
    int matchedLineCount = 0;
    int totalLineCount = 0;
    
    // 条码识别结果
    QVector<BarcodeResult> barcodeResults;
    QString barcodeStatus;  // 识别状态信息

    cv::Mat getFinalDisplay() const;

private:
    cv::Mat overlayMaskOnImage(const cv::Mat& bgr,const cv::Mat& mask) const;
};

class IPipelineStep
{
public:
    virtual ~IPipelineStep()=default;
    virtual void run(PipelineContext& ctx)=0;

};

class Pipeline
{
public:
    Pipeline();
    
    // 添加Pipeline步骤
    void add(std::unique_ptr<IPipelineStep> step);
    
    // 执行所有步骤
    void run(PipelineContext& ctx);
    
    // 获取步骤数量
    size_t stepCount() const;
    
    // 清空所有步骤
    void clear();

private:
    std::vector<std::unique_ptr<IPipelineStep>> steps_;
};
