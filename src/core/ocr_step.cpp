#include "ocr_step.h"
#include "config/pipeline_config.h"
#include "OcrLite.h"
#include <opencv2/imgproc.hpp>
#include <QDir>
#include <QCoreApplication>
#include <spdlog/spdlog.h>

StepOcrRecognition::StepOcrRecognition() = default;
StepOcrRecognition::~StepOcrRecognition() = default;

void StepOcrRecognition::initRapidOcr()
{
    if (m_initialized) return;

    m_ocr = std::make_unique<OcrLite>();
    m_ocr->setNumThread(4);

    // 模型路径：相对于可执行文件
    QString appDir = QCoreApplication::applicationDirPath();
    QString modelsDir = appDir + "/resources/ocr_models";

    // 如果 appDir 下没有，尝试从项目根目录找
    if (!QDir(modelsDir).exists()) {
        modelsDir = QDir::currentPath() + "/resources/ocr_models";
    }

    std::string detPath  = (modelsDir + "/ch_PP-OCRv4_det_mobile.onnx").toStdString();
    std::string clsPath  = (modelsDir + "/ch_ppocr_mobile_v2.0_cls_mobile.onnx").toStdString();
    std::string recPath  = (modelsDir + "/ch_PP-OCRv4_rec_mobile.onnx").toStdString();
    std::string keysPath = (modelsDir + "/ppocr_keys_v1.txt").toStdString();

    spdlog::info("[OCR] Loading RapidOCR models from: {}", modelsDir.toStdString());
    spdlog::info("[OCR]   det:  {}", detPath);
    spdlog::info("[OCR]   cls:  {}", clsPath);
    spdlog::info("[OCR]   rec:  {}", recPath);
    spdlog::info("[OCR]   keys: {}", keysPath);

    bool ok = m_ocr->initModels(detPath, clsPath, recPath, keysPath);
    if (!ok) {
        spdlog::error("[OCR] Failed to init RapidOCR models!");
        m_ocr.reset();
        return;
    }

    m_initialized = true;
    spdlog::info("[OCR] RapidOCR initialized successfully");
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

    // 初始化 RapidOCR
    initRapidOcr();
    if (!m_ocr) {
        ctx.reason = "OCR初始化失败: 请检查ocr_models目录";
        return;
    }

    try {
        // RapidOCR 接受 BGR 图像，直接传入
        // 参数: padding, maxSideLen, boxScoreThresh, boxThresh, unClipRatio, doAngle, mostAngle
        OcrResult result = m_ocr->detect(
            src,
            50,                              // padding
            960,                             // maxSideLen
            0.5f,                            // boxScoreThresh
            cfg.confidenceThreshold,         // boxThresh (复用置信度阈值)
            1.6f,                            // unClipRatio
            true,                            // doAngle (启用方向检测)
            true                             // mostAngle
        );

        if (result.strRes.empty()) {
            ctx.ocrText = "";
            ctx.ocrRegions.clear();
            ctx.reason = "OCR: 未识别到文字";
            return;
        }

        // 设置识别文本
        ctx.ocrText = QString::fromStdString(result.strRes).trimmed();

        // 设置识别区域
        ctx.ocrRegions.clear();
        for (const auto& block : result.textBlocks) {
            OcrRegion region;

            // 从 boxPoint 计算包围盒
            if (!block.boxPoint.empty()) {
                int minX = block.boxPoint[0].x, maxX = block.boxPoint[0].x;
                int minY = block.boxPoint[0].y, maxY = block.boxPoint[0].y;
                for (size_t i = 1; i < block.boxPoint.size(); ++i) {
                    if (block.boxPoint[i].x < minX) minX = block.boxPoint[i].x;
                    if (block.boxPoint[i].x > maxX) maxX = block.boxPoint[i].x;
                    if (block.boxPoint[i].y < minY) minY = block.boxPoint[i].y;
                    if (block.boxPoint[i].y > maxY) maxY = block.boxPoint[i].y;
                }
                region.x = minX;
                region.y = minY;
                region.width = maxX - minX;
                region.height = maxY - minY;
            }

            region.text = QString::fromStdString(block.text);
            region.confidence = block.boxScore;
            ctx.ocrRegions.append(region);
        }

        ctx.reason = QString("OCR (RapidOCR): 识别 %1 个区域, 共 %2 字符")
                         .arg(ctx.ocrRegions.size())
                         .arg(ctx.ocrText.length());

        spdlog::info("[OCR] {}", ctx.reason.toStdString());
    }
    catch (const std::exception& ex) {
        ctx.reason = QString("OCR 异常: %1").arg(ex.what());
        spdlog::error("[OCR] {}", ctx.reason.toStdString());
    }
}
