#ifndef BARCODE_STEP_H
#define BARCODE_STEP_H

#include "pipeline.h"
#include "zxing_barcode_reader.h"

/**
 * 条码识别步骤类
 * 使用ZXing库进行条码识别，支持QR Code、Data Matrix、EAN-13
 */
class StepBarcodeRecognition : public IPipelineStep
{
public:
    explicit StepBarcodeRecognition(const BarcodeConfig* config);
    ~StepBarcodeRecognition();
    
    void run(PipelineContext& ctx) override;

private:
    const BarcodeConfig* cfg_;
    ZXingBarcodeReader reader_;  // ZXing条码读取器
    
    bool is2DCode(const QString& codeType) const;
    QString convertBarcodeType(const QString& zxingType) const;
};

#endif // BARCODE_STEP_H
