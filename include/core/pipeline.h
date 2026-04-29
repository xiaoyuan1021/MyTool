#pragma once
#include <opencv2/opencv.hpp>
#include <QVector>
#include <QVariantMap>
#include <QJsonObject>
#include <QPointF>
#include <QString>
#include <QRectF>
#include "config/pipeline_config.h"
#include "data/barcode_result.h"
#include "region_feature.h"
#include "display_config.h"
#include "algorithm/opencv_algorithm.h"

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

private:
    std::vector<std::unique_ptr<IPipelineStep>> steps_;
};
