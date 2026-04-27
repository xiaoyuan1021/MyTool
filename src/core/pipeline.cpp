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

size_t Pipeline::stepCount() const
{
    return steps_.size();
}

void Pipeline::clear()
{
    steps_.clear();
    qDebug() << "[Pipeline] 已清空所有步骤";
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
