#include "model_warmup.h"
#include "logger.h"
#include "OcrLite.h"
#include <QtConcurrent/QtConcurrent>
#include <QCoreApplication>
#include <QDir>

// ZXing headers
#include <zxing_barcode_reader.h>

void ModelWarmUp::warmUpAll()
{
    spdlog::info("[ModelWarmUp] 开始预热所有模型...");

    QFuture<void> future = QtConcurrent::run([]() {
        // 1. 预热 OCR
        warmUpOcr();

        // 2. 预热条码读取器
        warmUpBarcode();

        // 注意: 目标检测模型不在这里预热
        // 原因: OrtInference 是实例级别的，预热创建的临时对象会立即销毁，无实际效果
        // 目标检测模型由 ObjectDetectionTabWidget 按需加载

        spdlog::info("[ModelWarmUp] 所有模型预热完成");
    });
}

void ModelWarmUp::warmUpOcr(const QString& language)
{
    Q_UNUSED(language);

    QString appDir = QCoreApplication::applicationDirPath();
    QString modelsDir = appDir + "/resources/ocr_models";

    if (!QDir(modelsDir).exists()) {
        modelsDir = QDir::currentPath() + "/resources/ocr_models";
    }

    if (!QDir(modelsDir).exists()) {
        spdlog::warn("[ModelWarmUp] OCR预热失败: ocr_models 目录不存在");
        return;
    }

    std::string detPath  = (modelsDir + "/ch_PP-OCRv4_det_mobile.onnx").toStdString();
    std::string clsPath  = (modelsDir + "/ch_ppocr_mobile_v2.0_cls_mobile.onnx").toStdString();
    std::string recPath  = (modelsDir + "/ch_PP-OCRv4_rec_mobile.onnx").toStdString();
    std::string keysPath = (modelsDir + "/ppocr_keys_v1.txt").toStdString();

    OcrLite ocr;
    ocr.setNumThread(2);
    bool ok = ocr.initModels(detPath, clsPath, recPath, keysPath);

    if (!ok) {
        spdlog::warn("[ModelWarmUp] OCR预热失败: 模型加载失败 from {}", modelsDir.toStdString());
        return;
    }

    spdlog::info("[ModelWarmUp] OCR预热完成: RapidOCR PP-OCRv4 模型已加载");
}

void ModelWarmUp::warmUpBarcode()
{
    // ZXing 读取器是轻量级的，只需配置格式即可
    // 实际的 reader_ 实例在 StepBarcodeRecognition 中管理
    // 这里只是确保 ZXing 库可用
    ZXingBarcodeReader reader;
    QStringList formats = {"QRCode", "DataMatrix", "EAN-13"};
    reader.setFormats(formats);
    reader.setTryRotate(true);
    reader.setTryInvert(true);
    reader.setTryDownscale(true);

    spdlog::info("[ModelWarmUp] 条码读取器预热完成: 支持格式 {}", formats.join(", ").toStdString());
}
