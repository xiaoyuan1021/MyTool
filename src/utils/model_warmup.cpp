#include "model_warmup.h"
#include "logger.h"
#include <QtConcurrent/QtConcurrent>
#include <tesseract/baseapi.h>
#include <QDir>

// ZXing headers
#include <zxing_barcode_reader.h>

void ModelWarmUp::warmUpAll()
{
    spdlog::info("[ModelWarmUp] 开始预热所有模型...");

    QtConcurrent::run([]() {
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
    tesseract::TessBaseAPI api;
    QString tessdataPath = QDir::currentPath() + "/resources/tessdata";

    if (api.Init(tessdataPath.toUtf8().constData(),
                 language.toUtf8().constData(),
                 tesseract::OEM_DEFAULT)) {
        spdlog::warn("[ModelWarmUp] OCR预热失败: 无法加载tessdata from {}", tessdataPath.toStdString());
        return;
    }

    spdlog::info("[ModelWarmUp] OCR预热完成: 语言模型已加载 ({})", language.toStdString());
    api.End();
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
