#include "zxing_barcode_reader.h"
#include "logger.h"
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
        spdlog::info("[ZXing] 输入图像为空");
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
        spdlog::info("[ZXing] 图像格式错误，需要8位灰度图");
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

            spdlog::debug("[ZXing] corners: TL={},{} TR={},{} BR={},{} BL={},{} format={}",
                position.topLeft().x, position.topLeft().y,
                position.topRight().x, position.topRight().y,
                position.bottomRight().x, position.bottomRight().y,
                position.bottomLeft().x, position.bottomLeft().y,
                result.type.toStdString());

            if (position.topLeft().x != 0 || position.topLeft().y != 0 ||
                position.bottomRight().x != 0 || position.bottomRight().y != 0)
            {
                double xs[4] = {
                    static_cast<double>(position.topLeft().x),
                    static_cast<double>(position.topRight().x),
                    static_cast<double>(position.bottomRight().x),
                    static_cast<double>(position.bottomLeft().x)
                };
                double ys[4] = {
                    static_cast<double>(position.topLeft().y),
                    static_cast<double>(position.topRight().y),
                    static_cast<double>(position.bottomRight().y),
                    static_cast<double>(position.bottomLeft().y)
                };
                double x = *std::min_element(xs, xs + 4);
                double y = *std::min_element(ys, ys + 4);
                double maxX = *std::max_element(xs, xs + 4);
                double maxY = *std::max_element(ys, ys + 4);
                double w = maxX - x;
                double h = maxY - y;

                auto fmt = barcode.format();
                bool is1D = (fmt != ZXing::BarcodeFormat::QRCode &&
                             fmt != ZXing::BarcodeFormat::DataMatrix &&
                             fmt != ZXing::BarcodeFormat::Aztec &&
                             fmt != ZXing::BarcodeFormat::PDF417);

                if (is1D && w > 0 && h > 0)
                {
                    double padX = w * 0.08;
                    double padY = h * 0.5;
                    x = std::max(0.0, x - padX);
                    y = std::max(0.0, y - padY);
                    w = std::min(static_cast<double>(gray.cols) - x, w + padX * 2);
                    h = std::min(static_cast<double>(gray.rows) - y, h + padY * 2);
                }

                spdlog::debug("[ZXing] bbox: {} {} {} {} is1D={}", x, y, w, h, is1D);

                result.location = QRectF(x, y, w, h);
            }
            else
            {
                result.location = QRectF(0, 0, gray.cols, gray.rows);
            }

            result.quality = -1;

            results.append(result);

            spdlog::info("[ZXing] 识别到条码: {} 数据: {}", result.type.toStdString(), result.data.toStdString());
        }

        if (results.isEmpty())
        {
            spdlog::info("[ZXing] 未识别到条码");
        }
    }
    catch (const std::exception& ex)
    {
        spdlog::debug("[ZXing] 识别异常: {}", ex.what());
    }
    catch (...)
    {
        spdlog::info("[ZXing] 未知异常");
    }

    return results;
}
