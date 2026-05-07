#include "pipeline.h"
#include "logger.h"

// ========== Pipeline类实现 ==========

Pipeline::Pipeline() = default;

void Pipeline::add(std::unique_ptr<IPipelineStep> step)
{
    if (step) {
        steps_.push_back(std::move(step));
    } else {
        qDebug() << "[Pipeline] 警告：尝试添加空步骤";
    }
}

void Pipeline::run(PipelineContext& ctx)
{
    if (steps_.empty()) {
        qDebug() << "[Pipeline] 警告：没有步骤可执行";
        return;
    }

    qDebug() << "[Pipeline] 开始执行，共" << steps_.size() << "个步骤";
    
    for (size_t i = 0; i < steps_.size(); ++i) {
        auto& step = steps_[i];
        if (step) {
            step->run(ctx);
        } else {
            qDebug() << "[Pipeline] 警告：步骤" << (i + 1) << "为空，跳过";
        }
    }
}

cv::Mat maskToGreenWhite(const cv::Mat &mask)
{
    if (mask.empty()) return cv::Mat();

    try {
        cv::Mat m;
        if (mask.type() != CV_8U) {
            mask.convertTo(m, CV_8U);
        } else {
            m = mask;
        }

        cv::Mat result(m.rows, m.cols, CV_8UC3);

        for (int y = 0; y < m.rows; y++)
        {
            const uchar* mp = m.ptr<uchar>(y);
            cv::Vec3b* rp = result.ptr<cv::Vec3b>(y);
            for (int x = 0; x < m.cols; ++x)
            {
                if (mp[x] == 0)
                {
                    rp[x][0] = 0;
                    rp[x][1] = 255;
                    rp[x][2] = 0;
                }
                else
                {
                    rp[x][0] = 255;
                    rp[x][1] = 255;
                    rp[x][2] = 255;
                }
            }
        }

        return result;
    } catch (const cv::Exception& ex) {
        qDebug() << "[maskToGreenWhite] OpenCV错误:" << ex.what();
        Logger::instance()->error(QString("maskToGreenWhite转换错误: %1").arg(ex.what()));
        return cv::Mat();
    } catch (...) {
        qDebug() << "[maskToGreenWhite] 未知异常";
        Logger::instance()->error("maskToGreenWhite转换未知异常");
        return cv::Mat();
    }
}

cv::Mat PipelineContext::getFinalDisplay() const
{
    try {
        using Mode = DisplayConfig::Mode;

        switch (displayConfig.mode)
        {
        case Mode::Original:
            return srcBgr.empty() ? cv::Mat() : srcBgr;

        case Mode::Channel:
            if (!channelImg.empty())
            {
                return channelImg;
            }
            else
            {
                return srcBgr;
            }
        case Mode::Enhanced:
            if (!enhanced.empty()) {
                if (enhanced.channels() == 1) {
                    cv::Mat bgr;
                    cv::cvtColor(enhanced, bgr, cv::COLOR_GRAY2BGR);
                    return bgr;
                }
                return enhanced;
            }
            return srcBgr;

        case Mode::MaskGreenWhite:
            if (!mask.empty()) {
                return OpenCVAlgorithm::convertToGreenWhite(mask);
            }
            if (!processed.empty()) {
                return OpenCVAlgorithm::convertToGreenWhite(processed);
            }
            return srcBgr;

        case Mode::MaskOverlay:
            if (!mask.empty()) {
                return overlayMaskOnImage(srcBgr, mask);
            }
            return srcBgr;

        case Mode::Processed:
            if (!processed.empty()) {
                if (processed.channels() == 1) {
                    return OpenCVAlgorithm::convertToGreenWhite(processed);
                }
                return processed;
            }
            return srcBgr;
        case Mode::LineDetect:
            if (!lineDetect.empty())
            {
                return lineDetect;
            }
            return srcBgr;

        case Mode::BarcodeOverlay:
            if (!barcodeResults.isEmpty() && !srcBgr.empty())
            {
                return drawBarcodeOverlay(srcBgr, barcodeResults);
            }
            return srcBgr;

        case Mode::MaskOnly:
            if (!mask.empty())
            {
                if (mask.channels() == 1) {
                    cv::Mat bgr;
                    cv::cvtColor(mask, bgr, cv::COLOR_GRAY2BGR);
                    return bgr;
                }
                return mask;
            }
            return srcBgr;

        case Mode::ProcessedOverlay:
            if (!processed.empty() && !srcBgr.empty())
            {
                cv::Mat overlayMask;
                if (processed.channels() == 1) {
                    overlayMask = processed;
                } else {
                    cv::cvtColor(processed, overlayMask, cv::COLOR_BGR2GRAY);
                }
                return overlayMaskOnImage(srcBgr, overlayMask);
            }
            return srcBgr;

        default:
            return srcBgr;
        }
    } catch (const cv::Exception& ex) {
        qDebug() << "[Pipeline] 显示图像转换错误:" << ex.what();
        Logger::instance()->error(QString("显示图像转换错误: %1").arg(ex.what()));
        return srcBgr.empty() ? cv::Mat() : srcBgr;
    } catch (...) {
        qDebug() << "[Pipeline] 显示图像转换未知异常";
        Logger::instance()->error("显示图像转换未知异常");
        return srcBgr.empty() ? cv::Mat() : srcBgr;
    }
}


cv::Mat PipelineContext::overlayMaskOnImage(const cv::Mat &bgr, const cv::Mat &mask) const
{
    if (bgr.empty() || mask.empty()) return bgr;

    try {
        if (bgr.size() != mask.size()) {
            qDebug() << "[overlayMaskOnImage] 尺寸不匹配";
            return bgr;
        }

        cv::Mat m;
        if (mask.type() != CV_8U) {
            mask.convertTo(m, CV_8U);
        } else {
            m = mask;
        }

        cv::Mat result = bgr.clone();

        for (int y = 0; y < m.rows; y++)
        {
            const uchar* mp = m.ptr<uchar>(y);
            cv::Vec3b* rp = result.ptr<cv::Vec3b>(y);

            for (int x = 0; x < m.cols; ++x)
            {
                if (mp[x] == 0)
                {
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

cv::Mat PipelineContext::drawBarcodeOverlay(const cv::Mat& bgr, const QVector<BarcodeResult>& barcodes) const
{
    if (barcodes.isEmpty() || bgr.empty()) return bgr;

    try {
        cv::Mat result = bgr.clone();

        for (const auto& bc : barcodes) {
            QRectF rect = bc.location;
            cv::Rect roi(cv::Point(static_cast<int>(rect.x()), static_cast<int>(rect.y())),
                         cv::Size(static_cast<int>(rect.width()), static_cast<int>(rect.height())));

            // 绘制绿色矩形框
            cv::rectangle(result, roi, cv::Scalar(0, 255, 0), 2);

            // 绘制标签背景
            QString label = QString("[%1] %2").arg(bc.type).arg(bc.data);
            if (bc.quality > 0) {
                label += QString(" (%.0f%%)").arg(bc.quality);
            }
            std::string text = label.toStdString();

            int baseline = 0;
            cv::Size textSize = cv::getTextSize(text, cv::FONT_HERSHEY_SIMPLEX, 0.5, 1, &baseline);
            cv::Point labelOrigin(roi.x, std::max(roi.y - 5, textSize.height + 5));
            cv::rectangle(result,
                          cv::Rect(labelOrigin.x, labelOrigin.y - textSize.height - 4,
                                   textSize.width + 6, textSize.height + 6),
                          cv::Scalar(0, 0, 0), cv::FILLED);

            // 绘制白色文字
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
