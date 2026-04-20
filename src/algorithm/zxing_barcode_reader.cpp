#include "zxing_barcode_reader.h"
#include <QDebug>
#include <ReadBarcode.h>
#include <ImageView.h>
#include <ReaderOptions.h>
#include <BarcodeFormat.h>
#include <opencv2/opencv.hpp>

ZXingBarcodeReader::ZXingBarcodeReader()
    : tryRotate_(true)
    , tryInvert_(true)
    , tryDownscale_(true)
{
    // 默认支持的条码格式
    formats_ = {"QRCode", "DataMatrix", "EAN-13"};
}

ZXingBarcodeReader::~ZXingBarcodeReader()
{
}

void ZXingBarcodeReader::setFormats(const QStringList& formats)
{
    formats_ = formats;
}

void ZXingBarcodeReader::setTryRotate(bool enable)
{
    tryRotate_ = enable;
}

void ZXingBarcodeReader::setTryInvert(bool enable)
{
    tryInvert_ = enable;
}

void ZXingBarcodeReader::setTryDownscale(bool enable)
{
    tryDownscale_ = enable;
}

QVector<ZXingBarcodeResult> ZXingBarcodeReader::readBarcodes(const cv::Mat& image)
{
    if (image.empty())
    {
        qDebug() << "[ZXing] 输入图像为空";
        return {};
    }

    cv::Mat gray;
    
    // 转换为灰度图
    if (image.channels() == 3)
    {
        cv::cvtColor(image, gray, cv::COLOR_BGR2GRAY);
    }
    else if (image.channels() == 4)
    {
        cv::cvtColor(image, gray, cv::COLOR_BGRA2GRAY);
    }
    else
    {
        gray = image.clone();
    }

    return decodeImage(gray);
}

QVector<ZXingBarcodeResult> ZXingBarcodeReader::decodeImage(const cv::Mat& gray)
{
    QVector<ZXingBarcodeResult> results;

    if (gray.empty() || gray.type() != CV_8UC1)
    {
        qDebug() << "[ZXing] 图像格式错误，需要8位灰度图";
        return results;
    }

    try
    {
        // 确保图像连续
        cv::Mat continuous;
        if (!gray.isContinuous())
            continuous = gray.clone();
        else
            continuous = gray;

        // 创建 ImageView
        ZXing::ImageView imageView(
            continuous.data,
            continuous.cols,
            continuous.rows,
            ZXing::ImageFormat::Lum
        );

        // 配置读取选项
        ZXing::ReaderOptions options;

        // -----------------------------
        // 设置识别格式（新版 ZXing 写法）
        // -----------------------------
        ZXing::BarcodeFormats formats;

        for (const QString& fmt : formats_)
        {
            if (fmt == "QRCode" || fmt == "QR Code" || fmt == "QRCODE")
            {
                formats |= ZXing::BarcodeFormat::QRCode;
            }
            else if (fmt == "DataMatrix" || fmt == "Data Matrix" || fmt == "DATAMATRIX")
            {
                formats |= ZXing::BarcodeFormat::DataMatrix;
            }
            else if (fmt == "EAN-13" || fmt == "EAN13")
            {
                formats |= ZXing::BarcodeFormat::EAN13;
            }
            else if (fmt == "EAN-8" || fmt == "EAN8")
            {
                formats |= ZXing::BarcodeFormat::EAN8;
            }
            else if (fmt == "Code128" || fmt == "Code 128")
            {
                formats |= ZXing::BarcodeFormat::Code128;
            }
            else if (fmt == "Code39" || fmt == "Code 39")
            {
                formats |= ZXing::BarcodeFormat::Code39;
            }
        }

        // 如果用户没指定格式，则识别全部
        if (formats.empty())
            formats = ZXing::BarcodeFormat::Any;

        options.setFormats(formats);

        // 其他选项
        options.setTryRotate(tryRotate_);
        options.setTryInvert(tryInvert_);
        options.setTryDownscale(tryDownscale_);
        options.setTryHarder(true);

        // 执行识别
        auto barcodes = ZXing::ReadBarcodes(imageView, options);

        // 转换结果
        for (const auto& barcode : barcodes)
        {
            ZXingBarcodeResult result;

            result.data = QString::fromUtf8(barcode.text().c_str());
            result.type = QString::fromUtf8(ZXing::ToString(barcode.format()).c_str());

            auto position = barcode.position();

            if (position.topLeft().x != 0 || position.topLeft().y != 0 ||
                position.bottomRight().x != 0 || position.bottomRight().y != 0)
            {
                double x = std::min(position.topLeft().x, position.bottomLeft().x);
                double y = std::min(position.topLeft().y, position.topRight().y);
                double w = std::abs(position.bottomRight().x - position.topLeft().x);
                double h = std::abs(position.bottomRight().y - position.topLeft().y);

                result.location = QRectF(x, y, w, h);
            }
            else
            {
                result.location = QRectF(0, 0, gray.cols, gray.rows);
            }

            result.quality = -1;

            results.append(result);

            qDebug() << "[ZXing] 识别到条码:"
                     << result.type
                     << "数据:"
                     << result.data;
        }

        if (results.isEmpty())
        {
            qDebug() << "[ZXing] 未识别到条码";
        }
    }
    catch (const std::exception& ex)
    {
        qDebug() << "[ZXing] 识别异常:" << ex.what();
    }
    catch (...)
    {
        qDebug() << "[ZXing] 未知异常";
    }

    return results;
}