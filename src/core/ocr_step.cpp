#include "ocr_step.h"
#include "config/pipeline_config.h"
#include <opencv2/imgproc.hpp>
#include <tesseract/baseapi.h>
#include <QDir>
#include <QString>
#include <QVector>
#include <memory>

void StepOcrRecognition::run(PipelineContext& ctx)
{
    if (!ctx.config) return;

    const auto& cfg = ctx.config->ocr;

    // 获取输入图像（优先使用enhanced，否则使用srcBgr）
    cv::Mat src = ctx.enhanced.empty() ? ctx.srcBgr : ctx.enhanced;
    if (src.empty()) return;

    // 转灰度
    cv::Mat gray;
    if (src.channels() == 3) {
        cv::cvtColor(src, gray, cv::COLOR_BGR2GRAY);
    } else {
        gray = src;
    }

    // 初始化Tesseract
    std::unique_ptr<tesseract::TessBaseAPI> api(new tesseract::TessBaseAPI());
    
    // 获取tessdata路径
    QString tessdataPath = QDir::currentPath() + "/tessdata";
    
    if (api->Init(tessdataPath.toUtf8().constData(), 
                  cfg.language.toUtf8().constData(), 
                  tesseract::OEM_DEFAULT)) {
        if (api->Init(NULL, cfg.language.toUtf8().constData(), tesseract::OEM_DEFAULT)) {
            ctx.reason = "OCR初始化失败: 请检查tessdata目录";
            return;
        }
    }

    // 根据用户选择设置PSM模式
    int psmMode;
    switch (cfg.pageMode) {
    case 1:  // 单行文字
        psmMode = 7;  // PSM_SINGLE_LINE
        break;
    case 2:  // 多行文字
        psmMode = 6;  // PSM_SINGLE_BLOCK
        break;
    default: // 自动检测
        psmMode = 3;  // PSM_AUTO
        break;
    }
    api->SetVariable("tessedit_pageseg_mode", std::to_string(psmMode).c_str());

    // 设置图像
    api->SetImage(gray.data, static_cast<int>(gray.cols), static_cast<int>(gray.rows), 
                  gray.channels(), static_cast<int>(gray.step));

    // 获取识别结果
    char* text = api->GetUTF8Text();
    if (text) {
        QString result = QString::fromUtf8(text).trimmed();
        if (!result.isEmpty()) {
            ctx.ocrText = result;
            
            // 获取详细结果（包含位置信息）
            std::unique_ptr<tesseract::ResultIterator> ri(api->GetIterator());
            if (ri) {
                do {
                    int left, top, right, bottom;
                    ri->BoundingBox(tesseract::RIL_BLOCK, &left, &top, &right, &bottom);
                    
                    OcrRegion region;
                    region.x = left;
                    region.y = top;
                    region.width = right - left;
                    region.height = bottom - top;
                    region.text = QString::fromUtf8(ri->GetUTF8Text(tesseract::RIL_BLOCK));
                    region.confidence = ri->Confidence(tesseract::RIL_BLOCK) / 100.0;
                    
                    ctx.ocrRegions.append(region);
                } while (ri->Next(tesseract::RIL_BLOCK));
            }
        }
        delete[] text;
    }

    api->End();
}
