#pragma once

#include "pipeline.h"
#include "data/ocr_region.h"
#include <QString>
#include <QVector>
#include <memory>

namespace tesseract {
class TessBaseAPI;
}

class StepOcrRecognition : public IPipelineStep
{
public:
    StepOcrRecognition();
    ~StepOcrRecognition() override;
    void run(PipelineContext& ctx) override;
    StepType stepType() const override { return StepType::OcrRecognition; }

private:
    static cv::Mat deskew(const cv::Mat& gray, cv::Mat& rotMatrixOut);
    static cv::Mat enhanceContrast(const cv::Mat& gray);
    void initTesseract(const QString& language);

    std::unique_ptr<tesseract::TessBaseAPI> m_api;
    QString m_cachedLanguage;
};
