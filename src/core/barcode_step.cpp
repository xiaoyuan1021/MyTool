#include "barcode_step.h"
#include "logger.h"
#include <opencv2/imgproc.hpp>

StepBarcodeRecognition::StepBarcodeRecognition()
{
    // 默认支持所有格式
    QStringList formats = {"QRCode", "DataMatrix", "EAN-13"};

    reader_.setFormats(formats);
    reader_.setTryRotate(true);
    reader_.setTryInvert(true);
    reader_.setTryDownscale(true);

    spdlog::info("[Barcode] ZXing条码读取器初始化完成，支持格式: {}", formats.join(", ").toStdString());
}

StepBarcodeRecognition::~StepBarcodeRecognition()
{
}

bool StepBarcodeRecognition::is2DCode(const QString& codeType) const
{
    return (codeType == "QR Code" || codeType == "Data Matrix" ||
            codeType == "QRCode" || codeType == "DataMatrix");
}

QString StepBarcodeRecognition::convertBarcodeType(const QString& zxingType) const
{
    if (zxingType == "QRCode") return "QR Code";
    if (zxingType == "DataMatrix") return "Data Matrix";
    if (zxingType == "EAN13") return "EAN-13";
    if (zxingType == "EAN8") return "EAN-8";
    if (zxingType == "UPCA") return "UPC-A";
    if (zxingType == "UPCE") return "UPC-E";
    if (zxingType == "Code128") return "Code 128";
    if (zxingType == "Code39") return "Code 39";
    return zxingType;
}

void StepBarcodeRecognition::run(PipelineContext& ctx)
{
    if (!ctx.config || !ctx.config->barcode.enableBarcode)
    {
        return;
    }

    cv::Mat input;
    if (!ctx.extractedMask.empty())
    {
        input = ctx.extractedMask;
    }
    else if (!ctx.enhanced.empty())
    {
        input = ctx.enhanced;
    }
    else
    {
        input = ctx.srcBgr;
    }

    if (input.empty())
    {
        ctx.barcodeStatus = "输入图像为空";
        return;
    }

    try
    {
        ctx.barcodeResults.clear();

        QVector<ZXingBarcodeResult> zxingResults = reader_.readBarcodes(input);

        for (const auto& zxingResult : zxingResults)
        {
            BarcodeResult result;
            result.data = zxingResult.data;
            result.type = convertBarcodeType(zxingResult.type);
            result.location = zxingResult.location;
            result.quality = zxingResult.quality;

            ctx.barcodeResults.append(result);
        }

        int numFound = ctx.barcodeResults.size();

        if (numFound > 0)
        {
            ctx.barcodeStatus = QString("成功识别 %1 个条码").arg(numFound);
        }
        else
        {
            ctx.barcodeStatus = "未检测到条码";
        }

        spdlog::info("[Barcode] {}", ctx.barcodeStatus.toStdString());
    }
    catch (const std::exception& ex)
    {
        QString error = QString("条码识别错误: %1").arg(ex.what());
        ctx.barcodeStatus = error;
        spdlog::info("[Barcode] {}", error.toStdString());
    }
    catch (...)
    {
        QString error = "条码识别发生未知错误";
        ctx.barcodeStatus = error;
        spdlog::info("[Barcode] {}", error.toStdString());
    }
}
