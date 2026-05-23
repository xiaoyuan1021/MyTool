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

    // 获取输入图像
    cv::Mat src = ctx.enhanced.empty() ? (ctx.channelImg.empty() ? ctx.srcBgr : ctx.channelImg) : ctx.enhanced;
    if (src.empty()) return;

    // 转灰度
    cv::Mat gray;
    if (src.channels() == 3) {
        cv::cvtColor(src, gray, cv::COLOR_BGR2GRAY);
    } else {
        gray = src;
    }

    // 预处理：二值化
    cv::Mat binary;
    if (cfg.enablePreprocess) {
        cv::threshold(gray, binary, cfg.binaryThreshold, 255, cv::THRESH_BINARY);
    } else {
        binary = gray;
    }

    // 初始化Tesseract
    std::unique_ptr<tesseract::TessBaseAPI> api(new tesseract::TessBaseAPI());
    
    // 获取tessdata路径（程序运行目录下的tessdata文件夹）
    QString tessdataPath = QDir::currentPath() + "/tessdata";
    
    if (api->Init(tessdataPath.toUtf8().constData(), 
                  cfg.language.toUtf8().constData(), 
                  tesseract::OEM_DEFAULT)) {
        // 初始化失败，尝试使用默认路径
        if (api->Init(NULL, cfg.language.toUtf8().constData(), tesseract::OEM_DEFAULT)) {
            ctx.reason = "OCR初始化失败: 请检查tessdata目录";
            return;
        }
    }

    // 设置图像
    api->SetImage(binary.data, binary.cols, binary.rows, 
                  binary.channels(), binary.step);

    // 获取识别结果
    char* text = api->GetUTF8Text();
    if (text) {
        QString result = QString::fromUtf8(text).trimmed();
        if (!result.isEmpty()) {
            // 将识别结果存储到扩展字段
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
