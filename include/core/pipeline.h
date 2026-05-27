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
#include "data/ocr_region.h"
#include "region_feature.h"
#include "display_config.h"

struct PipelineContext
{
    // ========== 输入 ==========
    const PipelineConfig* config = nullptr;  // 配置指针
    cv::Mat srcBgr;                          // 原图(3通道)

    // ========== 图像处理流水线 ==========
    cv::Mat channelImg;      // 颜色通道输出（灰度/HSV等）
    cv::Mat enhanced;        // 增强后图像
    cv::Mat filteredImage;   // 滤波去噪输出（StepImageFilter）
    cv::Mat ocrInputImage;   // OCR输入图像（预留）

    // ========== 过滤结果 ==========
    cv::Mat filterMask;      // 统一过滤结果（StepFilter）

    // ========== 筛选结果 ==========
    cv::Mat extractedMask;   // 形状筛选结果（StepShapeFilter）
    int regionCount = 0;     // 筛选后区域数
    std::vector<RegionFeature> regionFeatures;  // 区域特征

    // ========== 专项检测结果 ==========
    // 直线检测
    cv::Mat lineDetectImage;     // 直线检测结果图像
    int matchedLineCount = 0;    // 匹配的参考线数
    int totalLineCount = 0;      // 检测到的总直线数

    // 条码识别
    QVector<BarcodeResult> barcodeResults;
    QString barcodeStatus;

    // OCR识别
    QString ocrText;
    QVector<OcrRegion> ocrRegions;

    // ========== 可视化输出 ==========
    cv::Mat visualBase;       // 当前Tab应显示的图像

    // ========== 状态 ==========
    bool pass = true;
    QString reason;           // 状态/调试信息
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
    
    // ========== 步骤管理 ==========
    
    // 添加Pipeline步骤
    void add(std::unique_ptr<IPipelineStep> step);
    
    // 移除指定索引的步骤
    bool remove(int index);
    
    // 清空所有步骤
    void clear();
    
    // 获取步骤数量
    size_t size() const;
    
    // 检查是否为空
    bool empty() const;
    
    // 交换两个步骤的位置
    bool swap(int index1, int index2);
    
    // 获取指定索引的步骤（const版本）
    const IPipelineStep* getStep(int index) const;
    
    // 获取指定索引的步骤（非const版本）
    IPipelineStep* getStep(int index);
    
    // 执行所有步骤
    void run(PipelineContext& ctx);

private:
    std::vector<std::unique_ptr<IPipelineStep>> steps_;
};
