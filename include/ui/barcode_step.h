#ifndef BARCODE_STEP_H
#define BARCODE_STEP_H

#include "pipeline.h"
#include <HalconCpp.h>

using namespace HalconCpp;

/**
 * 条码识别步骤类
 * 使用Halcon API进行条码识别
 */
class StepBarcodeRecognition : public IPipelineStep
{
public:
    explicit StepBarcodeRecognition(const BarcodeConfig* config);
    ~StepBarcodeRecognition();
    
    void run(PipelineContext& ctx) override;

private:
    const BarcodeConfig* cfg_;
    HBarCode barcodeModel_;      // Halcon一维条码模型
    HDataCode2D dataCodeModel_;  // Halcon二维数据码模型（用于QR Code等）
    
    bool is2DCode(const QString& codeType) const;
};

#endif // BARCODE_STEP_H
