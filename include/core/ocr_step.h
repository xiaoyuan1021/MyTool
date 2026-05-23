#pragma once

#include "pipeline.h"
#include "data/ocr_region.h"
#include <QString>
#include <QVector>

/**
 * @brief OCR文字识别步骤
 *
 * 基于Tesseract OCR引擎，识别图像中的文字
 */
class StepOcrRecognition : public IPipelineStep
{
public:
    void run(PipelineContext& ctx) override;
};
