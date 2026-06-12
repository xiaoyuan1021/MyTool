#pragma once

#include "pipeline.h"
#include "data/ocr_region.h"
#include <QString>
#include <QVector>
#include <memory>

class OcrLite;

class StepOcrRecognition : public IPipelineStep
{
public:
    StepOcrRecognition();
    ~StepOcrRecognition() override;
    void run(PipelineContext& ctx) override;
    StepType stepType() const override { return StepType::OcrRecognition; }

private:
    void initRapidOcr();

    std::unique_ptr<OcrLite> m_ocr;
    bool m_initialized = false;
};
