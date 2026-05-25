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
    // 输入
    const PipelineConfig* config = nullptr;  // 步骤从这里读取配置参数
    cv::Mat srcBgr;      // 原图(3通道)
    cv::Mat visualBase;  // 最新的可视化结果（用于叠加类显示模式的基底）
    // 中间结果
    cv::Mat channelImg;  // 颜色通道输出（一般是灰度）
    cv::Mat enhanced;    // 增强后
    cv::Mat mask;        // 过滤得到的 mask (0/255)
    cv::Mat processed;   // 算法处理后的图（可能还是 mask / 或灰度）
    cv::Mat lineDetect;  // 直线检测结果
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

    // OCR识别结果
    QString ocrText;                    // 识别的完整文本
    QVector<OcrRegion> ocrRegions;      // 识别的区域列表
    
    // 步骤间显式数据流字段（用于解耦合）
    cv::Mat filteredImage;      // 滤波去噪输出（StepImageFilter -> 后续步骤）
    cv::Mat ocrInputImage;      // OCR输入图像（StepOcrPreprocess -> StepOcrRecognition）
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
