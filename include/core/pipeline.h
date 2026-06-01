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

/**
 * Pipeline执行上下文
 *
 * 生命周期：每次 PipelineManager::execute() 调用创建一个独立实例，
 *           各步骤按顺序填充字段，执行完毕后由调用方消费结果。
 *
 * 重要：此结构体的检测结果字段（regionCount, barcodeResults 等）仅在
 *       单次单个ROI的Pipeline执行期间有效。批量检测时，每个ROI会获得
 *       独立的 PipelineContext 实例，结果通过 RoiDetectionResult 汇总，
 *       不应跨ROI累加或混合这些字段。
 */
struct PipelineContext
{
    // ========== 输入（execute时设置）==========
    const PipelineConfig* config = nullptr;  ///< 配置指针（指向调用方传入的config副本）
    cv::Mat srcBgr;                          ///< 原图(3通道)，即ROI裁剪后的图像

    // ========== 图像处理流水线（各步骤按顺序填充）==========
    cv::Mat channelImg;      ///< 颜色通道输出（StepColorChannel）
    cv::Mat enhanced;        ///< 增强后图像（StepEnhance）
    cv::Mat filteredImage;   ///< 滤波去噪输出（StepImageFilter）
    cv::Mat ocrInputImage;   ///< OCR输入图像（预留）

    // ========== 过滤结果 ==========
    cv::Mat filterMask;      ///< 统一过滤结果（StepFilter）

    // ========== 筛选结果 ==========
    cv::Mat extractedMask;   ///< 形状筛选结果（StepShapeFilter）
    int regionCount = 0;     ///< 筛选后区域数（仅当ShapeFilter启用时有效）
    std::vector<RegionFeature> regionFeatures;  ///< 区域特征

    // ========== 专项检测结果 ==========
    // 直线检测
    cv::Mat lineDetectImage;     ///< 直线检测结果图像
    int matchedLineCount = 0;    ///< 匹配的参考线数
    int totalLineCount = 0;      ///< 检测到的总直线数

    // 条码识别
    QVector<BarcodeResult> barcodeResults;  ///< 条码识别结果（仅当Barcode步骤启用时有效）
    QString barcodeStatus;

    // OCR识别
    QString ocrText;            ///< OCR识别文本（仅当OCR步骤启用时有效）
    QVector<OcrRegion> ocrRegions;

    // 目标检测
    struct DetectionResult {
        QString label;
        float confidence;
        QRectF bbox;
    };
    QVector<DetectionResult> objectDetectionResults;

    // ========== 可视化输出 ==========
    cv::Mat visualBase;       ///< 当前Tab应显示的图像

    // ========== 状态 ==========
    bool pass = true;         ///< Pipeline是否执行成功（非检测判定）
    QString reason;           ///< 状态/调试信息
};

class IPipelineStep
{
public:
    virtual ~IPipelineStep()=default;
    virtual void run(PipelineContext& ctx)=0;
    virtual StepType stepType() const=0;
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
