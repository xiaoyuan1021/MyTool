#include "display_renderer.h"
#include "pipeline.h"
#include "algorithm/opencv_algorithm.h"
#include "logger.h"
#include <QImage>
#include <QPainter>
#include <QFont>
#include <QPen>

namespace {

cv::Mat ensureBgr(const cv::Mat& img)
{
    if (img.empty() || img.channels() == 3) return img;
    cv::Mat bgr;
    cv::cvtColor(img, bgr, cv::COLOR_GRAY2BGR);
    return bgr;
}

} // anonymous namespace

namespace DisplayRenderer {

cv::Mat render(const PipelineContext& ctx, DisplayConfig::Mode mode)
{
    using Mode = DisplayConfig::Mode;

    try {
        switch (mode)
        {
        case Mode::Original:
            return ctx.srcBgr.empty() ? cv::Mat() : ctx.srcBgr;

        case Mode::Channel:
            if (!ctx.channelImg.empty())
                return ctx.channelImg;
            return ensureBgr(ctx.visualBase);

        case Mode::Enhanced:
            if (!ctx.enhanced.empty()) {
                if (ctx.enhanced.channels() == 1) {
                    cv::Mat bgr;
                    cv::cvtColor(ctx.enhanced, bgr, cv::COLOR_GRAY2BGR);
                    return bgr;
                }
                return ctx.enhanced;
            }
            return ensureBgr(ctx.visualBase);

        case Mode::MaskGreenWhite:
            if (!ctx.filterMask.empty())
                return OpenCVAlgorithm::convertToGreenWhite(ctx.filterMask);
            if (!ctx.extractedMask.empty())
                return OpenCVAlgorithm::convertToGreenWhite(ctx.extractedMask);
            return ensureBgr(ctx.visualBase);

        case Mode::MaskOverlay:
            if (!ctx.extractedMask.empty())
                return overlayMaskOnImage(ensureBgr(ctx.visualBase), ctx.extractedMask);
            if (!ctx.filterMask.empty())
                return overlayMaskOnImage(ensureBgr(ctx.visualBase), ctx.filterMask);
            return ensureBgr(ctx.visualBase);

        case Mode::Processed:
            if (!ctx.extractedMask.empty()) {
                if (ctx.extractedMask.channels() == 1)
                    return OpenCVAlgorithm::convertToGreenWhite(ctx.extractedMask);
                return ctx.extractedMask;
            }
            return ensureBgr(ctx.visualBase);

        case Mode::LineDetect:
            if (!ctx.lineDetectImage.empty())
                return ctx.lineDetectImage;
            return ensureBgr(ctx.visualBase);

        case Mode::BarcodeOverlay:
            {
                cv::Mat base = ensureBgr(ctx.visualBase);
                if (!base.empty()) {
                    if (!ctx.barcodeResults.isEmpty())
                        return drawBarcodeOverlay(base, ctx.barcodeResults);
                    return base;
                }
            }
            return ensureBgr(ctx.visualBase);

        case Mode::OcrOverlay:
            {
                cv::Mat base = ensureBgr(ctx.visualBase);
                if (!base.empty()) {
                    if (!ctx.ocrRegions.isEmpty())
                        return drawOcrOverlay(base, ctx.ocrRegions);
                    return base;
                }
            }
            return ensureBgr(ctx.visualBase);

        case Mode::MaskOnly:
            if (!ctx.filterMask.empty()) {
                if (ctx.filterMask.channels() == 1) {
                    cv::Mat bgr;
                    cv::cvtColor(ctx.filterMask, bgr, cv::COLOR_GRAY2BGR);
                    return bgr;
                }
                return ctx.filterMask;
            }
            return ensureBgr(ctx.visualBase);

        case Mode::ProcessedOverlay:
            if (!ctx.extractedMask.empty()) {
                cv::Mat overlayMask;
                if (ctx.extractedMask.channels() == 1)
                    overlayMask = ctx.extractedMask;
                else
                    cv::cvtColor(ctx.extractedMask, overlayMask, cv::COLOR_BGR2GRAY);
                cv::Mat base = ensureBgr(ctx.visualBase);
                if (!base.empty())
                    return overlayMaskOnImage(base, overlayMask);
            }
            return ensureBgr(ctx.visualBase);

        default:
            return ensureBgr(ctx.visualBase);
        }
    } catch (const cv::Exception& ex) {
Logger::instance()->error(QString("DisplayRenderer渲染错误: %1").arg(ex.what()));
        return ctx.srcBgr.empty() ? cv::Mat() : ctx.srcBgr;
    } catch (const std::exception& e) {
Logger::instance()->error(QString("DisplayRenderer渲染异常: %1").arg(e.what()));
        return ctx.srcBgr.empty() ? cv::Mat() : ctx.srcBgr;
    } catch (...) {
        Logger::instance()->info("[DisplayRenderer] 未知异常");
        Logger::instance()->error("DisplayRenderer渲染未知异常");
        return ctx.srcBgr.empty() ? cv::Mat() : ctx.srcBgr;
    }
}


cv::Mat overlayMaskOnImage(const cv::Mat& bgr, const cv::Mat& mask, float alpha)
{
    Q_UNUSED(alpha); // 当前使用固定绿色叠加，alpha保留为后续扩展

    if (bgr.empty() || mask.empty()) return bgr;

    try {
        if (bgr.size() != mask.size()) {
            Logger::instance()->info("[overlayMaskOnImage] 尺寸不匹配");
            return bgr;
        }

        cv::Mat m;
        if (mask.type() != CV_8U)
            mask.convertTo(m, CV_8U);
        else
            m = mask;

        cv::Mat result = bgr.clone();

        for (int y = 0; y < m.rows; y++) {
            const uchar* mp = m.ptr<uchar>(y);
            cv::Vec3b* rp = result.ptr<cv::Vec3b>(y);

            for (int x = 0; x < m.cols; ++x) {
                if (mp[x] != 0) {
                    rp[x] = cv::Vec3b(0, 255, 0);
                }
            }
        }

        return result;
    } catch (const cv::Exception& ex) {
Logger::instance()->error(QString("overlayMaskOnImage错误: %1").arg(ex.what()));
        return bgr;
    } catch (...) {
        Logger::instance()->info("[overlayMaskOnImage] 未知异常");
        Logger::instance()->error("overlayMaskOnImage未知异常");
        return bgr;
    }
}


cv::Mat drawBarcodeOverlay(const cv::Mat& bgr, const QVector<BarcodeResult>& barcodes)
{
    if (barcodes.isEmpty() || bgr.empty()) return bgr;

    try {
        cv::Mat result = bgr.clone();

        for (const auto& bc : barcodes) {
            QRectF rect = bc.location;
            cv::Rect roi(cv::Point(static_cast<int>(rect.x()), static_cast<int>(rect.y())),
                         cv::Size(static_cast<int>(rect.width()), static_cast<int>(rect.height())));

            cv::rectangle(result, roi, cv::Scalar(0, 255, 0), 2);

            QString label = QString("[%1] %2").arg(bc.type).arg(bc.data);
            if (bc.quality > 0)
                label += QString(" (%.0f%%)").arg(bc.quality);
            std::string text = label.toStdString();

            int baseline = 0;
            cv::Size textSize = cv::getTextSize(text, cv::FONT_HERSHEY_SIMPLEX, 0.5, 1, &baseline);
            cv::Point labelOrigin(roi.x, std::max(roi.y - 5, textSize.height + 5));
            cv::rectangle(result,
                          cv::Rect(labelOrigin.x, labelOrigin.y - textSize.height - 4,
                                   textSize.width + 6, textSize.height + 6),
                          cv::Scalar(0, 0, 0), cv::FILLED);

            cv::putText(result, text, cv::Point(labelOrigin.x + 3, labelOrigin.y - 3),
                        cv::FONT_HERSHEY_SIMPLEX, 0.5, cv::Scalar(255, 255, 255), 1);
        }

        return result;
    } catch (const cv::Exception& ex) {
Logger::instance()->error(QString("drawBarcodeOverlay错误: %1").arg(ex.what()));
        return bgr;
    } catch (...) {
        Logger::instance()->info("[drawBarcodeOverlay] 未知异常");
        Logger::instance()->error("drawBarcodeOverlay未知异常");
        return bgr;
    }
}


cv::Mat drawOcrOverlay(const cv::Mat& bgr, const QVector<OcrRegion>& regions)
{
    if (regions.isEmpty() || bgr.empty()) return bgr;

    try {
        int w = bgr.cols;
        int h = bgr.rows;

        // 创建QImage，逐行拷贝（避免OpenCV和QImage行对齐方式不一致导致的黑框）
        QImage resultImg(w, h, QImage::Format_RGB888);
        cv::Mat rgb;
        cv::cvtColor(bgr, rgb, cv::COLOR_BGR2RGB);
        for (int y = 0; y < h; ++y) {
            memcpy(resultImg.scanLine(y), rgb.ptr(y), static_cast<size_t>(w) * 3);
        }

        QPainter painter(&resultImg);
        painter.setRenderHint(QPainter::Antialiasing);

        QFont font("Microsoft YaHei", 10);
        painter.setFont(font);

        for (const auto& region : regions) {
            QFontMetrics fm(font);
            QRect textRect = fm.boundingRect(region.text);

            // 先画标签背景（黑底），再画绿色边框，避免 brush 状态残留导致边框被填充
            int labelX = region.x;
            int labelY = std::max(region.y - 5, textRect.height() + 5);

            QString label = region.text;
            if (region.confidence > 0) {
                label += QString(" (%1%)").arg(region.confidence * 100, 0, 'f', 1);
            }
            if (label.length() > 30) {
                label = label.left(27) + "...";
            }
            textRect = fm.boundingRect(label);

            QRect bgRect(labelX, labelY - textRect.height() - 4,
                        textRect.width() + 6, textRect.height() + 6);
            painter.setPen(Qt::NoPen);
            painter.setBrush(QColor(0, 0, 0));
            painter.drawRect(bgRect);

            painter.setPen(QColor(255, 255, 255));
            painter.setBrush(Qt::NoBrush);
            painter.drawText(labelX + 3, labelY - 3, label);

            // 最后画绿色边框（brush 已重置为 NoBrush，不会被填充）
            painter.setPen(QPen(QColor(0, 255, 0), 2));
            painter.setBrush(Qt::NoBrush);
            painter.drawRect(region.x, region.y, region.width, region.height);
        }

        painter.end();

        // 逐行拷贝回cv::Mat（避免对齐问题）
        cv::Mat result(h, w, CV_8UC3);
        for (int y = 0; y < h; ++y) {
            memcpy(result.ptr(y), resultImg.scanLine(y), static_cast<size_t>(w) * 3);
        }
        cv::cvtColor(result, result, cv::COLOR_RGB2BGR);
        return result;
    } catch (const cv::Exception& ex) {
Logger::instance()->error(QString("drawOcrOverlay错误: %1").arg(ex.what()));
        return bgr;
    } catch (...) {
        Logger::instance()->info("[drawOcrOverlay] 未知异常");
        Logger::instance()->error("drawOcrOverlay未知异常");
        return bgr;
    }
}

} // namespace DisplayRenderer
