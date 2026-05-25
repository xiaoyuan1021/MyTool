#include "ocr_step.h"
#include "config/pipeline_config.h"
#include <opencv2/imgproc.hpp>
#include <tesseract/baseapi.h>
#include <QDir>
#include <QString>
#include <QVector>
#include <memory>
#include <algorithm>

StepOcrRecognition::StepOcrRecognition() = default;
StepOcrRecognition::~StepOcrRecognition() = default;

cv::Mat StepOcrRecognition::deskew(const cv::Mat& gray, cv::Mat& rotMatrixOut)
{
    cv::Mat binary;
    cv::threshold(gray, binary, 0, 255, cv::THRESH_BINARY_INV | cv::THRESH_OTSU);

    std::vector<cv::Point> points;
    cv::findNonZero(binary, points);

    if (points.size() < 100) {
        rotMatrixOut = cv::Mat();
        return gray;
    }

    cv::RotatedRect rect = cv::minAreaRect(points);
    float angle = rect.angle;
    if (rect.size.width < rect.size.height) {
        angle = 90 + angle;
    }

    if (std::abs(angle) < 0.5 || std::abs(angle) > 30) {
        rotMatrixOut = cv::Mat();
        return gray;
    }

    cv::Point2f center(gray.cols / 2.0f, gray.rows / 2.0f);
    rotMatrixOut = cv::getRotationMatrix2D(center, angle, 1.0);
    cv::Mat rotated;
    cv::warpAffine(gray, rotated, rotMatrixOut, gray.size(),
                   cv::INTER_CUBIC, cv::BORDER_REPLICATE);
    return rotated;
}

cv::Mat StepOcrRecognition::enhanceContrast(const cv::Mat& gray)
{
    cv::Mat result;
    cv::Ptr<cv::CLAHE> clahe = cv::createCLAHE(2.0, cv::Size(8, 8));
    clahe->apply(gray, result);
    return result;
}

void StepOcrRecognition::initTesseract(const QString& language)
{
    if (m_api && m_cachedLanguage == language) return;

    m_api = std::make_unique<tesseract::TessBaseAPI>();

    QString tessdataPath = QDir::currentPath() + "/tessdata";
    if (m_api->Init(tessdataPath.toUtf8().constData(),
                    language.toUtf8().constData(),
                    tesseract::OEM_DEFAULT)) {
        if (m_api->Init(nullptr, language.toUtf8().constData(), tesseract::OEM_DEFAULT)) {
            m_api.reset();
            return;
        }
    }
    m_cachedLanguage = language;
}

void StepOcrRecognition::run(PipelineContext& ctx)
{
    if (!ctx.config) return;

    const auto& cfg = ctx.config->ocr;

    // 优先使用OCR专用输入，其次使用滤波结果，最后使用增强结果
    cv::Mat src;
    if (!ctx.ocrInputImage.empty()) {
        src = ctx.ocrInputImage;
    } else if (!ctx.filteredImage.empty()) {
        src = ctx.filteredImage;
    } else {
        src = ctx.enhanced.empty() ? ctx.srcBgr : ctx.enhanced;
    }
    if (src.empty()) return;

    cv::Mat gray;
    if (src.channels() == 3) {
        cv::cvtColor(src, gray, cv::COLOR_BGR2GRAY);
    } else {
        gray = src;
    }

    // 1. 自动纠偏
    cv::Mat rotMatrix;
    gray = deskew(gray, rotMatrix);

    // 2. CLAHE自适应对比度增强
    gray = enhanceContrast(gray);

    if (!gray.isContinuous()) {
        gray = gray.clone();
    }

    // 3. 复用Tesseract实例
    initTesseract(cfg.language);
    if (!m_api) {
        ctx.reason = "OCR初始化失败: 请检查tessdata目录";
        return;
    }

    int psmMode;
    switch (cfg.pageMode) {
    case 1:  psmMode = tesseract::PSM_SINGLE_LINE;  break;
    case 2:  psmMode = tesseract::PSM_SINGLE_BLOCK;  break;
    default: psmMode = tesseract::PSM_AUTO;           break;
    }
    m_api->SetPageSegMode(static_cast<tesseract::PageSegMode>(psmMode));

    if (cfg.dpi > 0) {
        m_api->SetSourceResolution(static_cast<int>(cfg.dpi));
    }

    if (!cfg.whitelist.isEmpty()) {
        m_api->SetVariable("tessedit_char_whitelist", cfg.whitelist.toUtf8().constData());
    }

    m_api->SetVariable("language_model_penalty_non_dict_word", "0.1");
    m_api->SetVariable("language_model_penalty_non_freq_dict_word", "0.1");
    m_api->SetVariable("textord_heavy_nr", "1");
    m_api->SetVariable("tessedit_unrej_trust_wd", "1");

    m_api->SetImage(gray.data, static_cast<int>(gray.cols), static_cast<int>(gray.rows),
                    gray.channels(), static_cast<int>(gray.step));

    char* text = m_api->GetUTF8Text();
    if (text) {
        QString result = QString::fromUtf8(text).trimmed();
        if (!result.isEmpty()) {
            ctx.ocrText = result;

            std::unique_ptr<tesseract::ResultIterator> ri(m_api->GetIterator());
            if (ri) {
                QVector<OcrRegion> wordRegions;
                do {
                    const char* wordText = ri->GetUTF8Text(tesseract::RIL_WORD);
                    if (!wordText) continue;
                    QString wordStr = QString::fromUtf8(wordText).trimmed();
                    delete[] wordText;
                    if (wordStr.isEmpty()) continue;

                    int conf = ri->Confidence(tesseract::RIL_WORD);
                    double confidence = conf / 100.0;
                    if (confidence < cfg.confidenceThreshold) continue;

                    int left, top, right, bottom;
                    ri->BoundingBox(tesseract::RIL_WORD, &left, &top, &right, &bottom);

                    // 如果有纠偏，将坐标映射回原图
                    if (!rotMatrix.empty()) {
                        cv::Mat invRot;
                        cv::invertAffineTransform(rotMatrix, invRot);
                        std::vector<cv::Point2f> pts = {
                            cv::Point2f(static_cast<float>(left), static_cast<float>(top)),
                            cv::Point2f(static_cast<float>(right), static_cast<float>(bottom))
                        };
                        cv::transform(pts, pts, invRot);
                        left = static_cast<int>(pts[0].x);
                        top = static_cast<int>(pts[0].y);
                        right = static_cast<int>(pts[1].x);
                        bottom = static_cast<int>(pts[1].y);
                    }

                    OcrRegion region;
                    region.x = left;
                    region.y = top;
                    region.width = right - left;
                    region.height = bottom - top;
                    region.text = wordStr;
                    region.confidence = confidence;
                    wordRegions.append(region);
                } while (ri->Next(tesseract::RIL_WORD));

                if (!wordRegions.isEmpty()) {
                    std::sort(wordRegions.begin(), wordRegions.end(),
                              [](const OcrRegion& a, const OcrRegion& b) {
                                  if (std::abs(a.y - b.y) <= std::max(a.height, b.height) / 2)
                                      return a.x < b.x;
                                  return a.y < b.y;
                              });

                    auto isCjk = [](const QString& text) -> bool {
                        if (text.isEmpty()) return false;
                        QChar c = text.at(0);
                        return c.unicode() >= 0x4E00 && c.unicode() <= 0x9FFF;
                    };

                    QVector<OcrRegion> merged;
                    OcrRegion current = wordRegions.first();
                    for (int i = 1; i < wordRegions.size(); ++i) {
                        const auto& w = wordRegions[i];
                        bool sameLine = std::abs(w.y - current.y) <= std::max(current.height, w.height) / 2;
                        int gap = std::max(current.height, w.height);
                        bool adjacent = w.x <= current.x + current.width + gap;
                        if (sameLine && adjacent) {
                            int endX = current.x + current.width;
                            int endW = w.x + w.width;
                            current.width = std::max(endX, endW) - current.x;
                            current.height = std::max(current.y + current.height, w.y + w.height) - current.y;
                            if (!isCjk(current.text) || !isCjk(w.text)) {
                                current.text += " ";
                            }
                            current.text += w.text;
                            current.confidence = std::min(current.confidence, w.confidence);
                        } else {
                            merged.append(current);
                            current = w;
                        }
                    }
                    merged.append(current);
                    ctx.ocrRegions = merged;
                }
            }
        }
        delete[] text;
    }

    m_api->Clear();
}
