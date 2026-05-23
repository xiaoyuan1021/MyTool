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
            if (!ctx.mask.empty())
                return OpenCVAlgorithm::convertToGreenWhite(ctx.mask);
            if (!ctx.processed.empty())
                return OpenCVAlgorithm::convertToGreenWhite(ctx.processed);
            return ensureBgr(ctx.visualBase);

        case Mode::MaskOverlay:
            if (!ctx.mask.empty())
                return overlayMaskOnImage(ensureBgr(ctx.visualBase), ctx.mask);
            return ensureBgr(ctx.visualBase);

        case Mode::Processed:
            if (!ctx.processed.empty()) {
                if (ctx.processed.channels() == 1)
                    return OpenCVAlgorithm::convertToGreenWhite(ctx.processed);
                return ctx.processed;
            }
            return ensureBgr(ctx.visualBase);

        case Mode::LineDetect:
            if (!ctx.lineDetect.empty())
                return ctx.lineDetect;
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
            if (!ctx.mask.empty()) {
                if (ctx.mask.channels() == 1) {
                    cv::Mat bgr;
                    cv::cvtColor(ctx.mask, bgr, cv::COLOR_GRAY2BGR);
                    return bgr;
                }
                return ctx.mask;
            }
            return ensureBgr(ctx.visualBase);

        case Mode::ProcessedOverlay:
            if (!ctx.processed.empty()) {
                cv::Mat overlayMask;
                if (ctx.processed.channels() == 1)
                    overlayMask = ctx.processed;
                else
                    cv::cvtColor(ctx.processed, overlayMask, cv::COLOR_BGR2GRAY);
                cv::Mat base = ensureBgr(ctx.visualBase);
                if (!base.empty())
                    return overlayMaskOnImage(base, overlayMask);
            }
            return ensureBgr(ctx.visualBase);

        default:
            return ensureBgr(ctx.visualBase);
        }
    } catch (const cv::Exception& ex) {
        qDebug() << "[DisplayRenderer] OpenCV错误:" << ex.what();
        Logger::instance()->error(QString("DisplayRenderer渲染错误: %1").arg(ex.what()));
        return ctx.srcBgr.empty() ? cv::Mat() : ctx.srcBgr;
    } catch (...) {
        qDebug() << "[DisplayRenderer] 未知异常";
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
            qDebug() << "[overlayMaskOnImage] 尺寸不匹配";
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
                if (mp[x] == 0) {
                    rp[x] = cv::Vec3b(0, 255, 0);
                }
            }
        }

        return result;
    } catch (const cv::Exception& ex) {
        qDebug() << "[overlayMaskOnImage] OpenCV错误:" << ex.what();
        Logger::instance()->error(QString("overlayMaskOnImage错误: %1").arg(ex.what()));
        return bgr;
    } catch (...) {
        qDebug() << "[overlayMaskOnImage] 未知异常";
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
        qDebug() << "[drawBarcodeOverlay] OpenCV错误:" << ex.what();
        Logger::instance()->error(QString("drawBarcodeOverlay错误: %1").arg(ex.what()));
        return bgr;
    } catch (...) {
        qDebug() << "[drawBarcodeOverlay] 未知异常";
        Logger::instance()->error("drawBarcodeOverlay未知异常");
        return bgr;
    }
}


cv::Mat drawOcrOverlay(const cv::Mat& bgr, const QVector<OcrRegion>& regions)
{
    if (regions.isEmpty() || bgr.empty()) return bgr;

    try {
        // 将cv::Mat转换为QImage进行Qt绘制（支持中文）
        cv::Mat rgb;
        cv::cvtColor(bgr, rgb, cv::COLOR_BGR2RGB);
        QImage qimg(rgb.data, rgb.cols, rgb.rows, rgb.step, QImage::Format_RGB888);
        QImage resultImg = qimg.copy();  // 深拷贝

        QPainter painter(&resultImg);
        painter.setRenderHint(QPainter::Antialiasing);

        QFont font("Microsoft YaHei", 10);
        painter.setFont(font);

        for (const auto& region : regions) {
            // 绘制绿色边框
            QPen pen(QColor(0, 255, 0), 2);
            painter.setPen(pen);
            painter.drawRect(region.x, region.y, region.width, region.height);

            // 构造标签：文字 + 置信度
            QString label = region.text;
            if (region.confidence > 0) {
                label += QString(" (%1%)").arg(region.confidence * 100, 0, 'f', 1);
            }

            // 限制显示长度
            if (label.length() > 30) {
                label = label.left(27) + "...";
            }

            // 计算标签位置
            QFontMetrics fm(font);
            QRect textRect = fm.boundingRect(label);
            int labelX = region.x;
            int labelY = std::max(region.y - 5, textRect.height() + 5);

            // 绘制黑色背景
            QRect bgRect(labelX, labelY - textRect.height() - 4,
                        textRect.width() + 6, textRect.height() + 6);
            painter.setPen(Qt::NoPen);
            painter.setBrush(QColor(0, 0, 0));
            painter.drawRect(bgRect);

            // 绘制白色文字
            painter.setPen(QColor(255, 255, 255));
            painter.drawText(labelX + 3, labelY - 3, label);
        }

        painter.end();

        // 转换回cv::Mat
        cv::Mat result(cv::Size(resultImg.width(), resultImg.height()), CV_8UC3);
        cv::cvtColor(cv::Mat(resultImg.height(), resultImg.width(), CV_8UC3,
                             const_cast<uchar*>(resultImg.constBits()), resultImg.bytesPerLine()),
                     result, cv::COLOR_RGB2BGR);
        return result;
    } catch (const cv::Exception& ex) {
        qDebug() << "[drawOcrOverlay] OpenCV错误:" << ex.what();
        Logger::instance()->error(QString("drawOcrOverlay错误: %1").arg(ex.what()));
        return bgr;
    } catch (...) {
        qDebug() << "[drawOcrOverlay] 未知异常";
        Logger::instance()->error("drawOcrOverlay未知异常");
        return bgr;
    }
}

} // namespace DisplayRenderer
