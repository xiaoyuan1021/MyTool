#include "pipeline.h"

Pipeline::Pipeline() {}

cv::Mat maskToGreenWhite(const cv::Mat &mask)
{
    if (mask.empty()) return cv::Mat();

    cv::Mat m;
    if (mask.type() != CV_8U) {
        mask.convertTo(m, CV_8U);
    } else {
        m = mask;
    }

    // 创建输出图像（BGR格式）
    cv::Mat result(m.rows, m.cols, CV_8UC3);

    for (int y = 0; y < m.rows; y++)
    {
        const uchar* mp = m.ptr<uchar>(y);
        cv::Vec3b* rp = result.ptr<cv::Vec3b>(y);
        for (int x = 0; x < m.cols; ++x)
        {
            // ✅ 反转逻辑：0=绿色(目标), 255=白色(背景)
            if (mp[x] == 0)  // HRegion 内部 = 目标区域（焊点）
            {
                rp[x][0] = 0;    // B
                rp[x][1] = 255;  // G - 绿色
                rp[x][2] = 0;    // R
            }
            else  // HRegion 外部 = 背景区域
            {
                rp[x][0] = 255;  // B
                rp[x][1] = 255;  // G - 白色
                rp[x][2] = 255;  // R
            }
        }
    }

    return result;
}

cv::Mat PipelineContext::getFinalDisplay() const
{
    using Mode = DisplayConfig::Mode;

    switch (displayConfig.mode)
    {
    case Mode::Original:
        return srcBgr.empty() ? cv::Mat() : srcBgr;

    case Mode::Enhanced:
        if (!enhanced.empty()) {
            // 如果是单通道，转成彩色
            if (enhanced.channels() == 1) {
                cv::Mat bgr;
                cv::cvtColor(enhanced, bgr, cv::COLOR_GRAY2BGR);
                return bgr;
            }
            return enhanced;
        }
        return srcBgr;

    case Mode::MaskGreenWhite:
        // 优先显示 processed，其次 mask
        if (!processed.empty()) {
            return convertToGreenWhite(processed);
        }
        if (!mask.empty()) {
            return convertToGreenWhite(mask);
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
                return convertToGreenWhite(processed);
            }
            return processed;
        }
        return srcBgr;

    default:
        return srcBgr;
    }
}

cv::Mat PipelineContext::convertToGreenWhite(const cv::Mat &mask) const
{
    if (mask.empty()) return cv::Mat();

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
            if (mp[x] == 0)  // 目标区域
            {
                rp[x] = cv::Vec3b(0, 255, 0);  // 绿色
            }
            else
            {
                rp[x] = cv::Vec3b(255, 255, 255);  // 白色
            }
        }
    }

    return result;
}

cv::Mat PipelineContext::overlayMaskOnImage(const cv::Mat &bgr, const cv::Mat &mask) const
{
    if (bgr.empty() || mask.empty()) return bgr;

    // 确保尺寸匹配
    if (bgr.size() != mask.size()) {
        qDebug() << "[overlayMaskOnImage] 尺寸不匹配";
        return bgr;
    }

    cv::Mat result = bgr.clone();

    // 创建绿色叠加层
    cv::Mat greenOverlay = cv::Mat::zeros(bgr.size(), CV_8UC3);
    greenOverlay.setTo(cv::Scalar(0, 255, 0));  // 绿色

    // 使用 mask 进行混合
    float alpha = displayConfig.overlayAlpha;
    for (int y = 0; y < mask.rows; y++)
    {
        const uchar* mp = mask.ptr<uchar>(y);
        cv::Vec3b* rp = result.ptr<cv::Vec3b>(y);
        const cv::Vec3b* gp = greenOverlay.ptr<cv::Vec3b>(y);

        for (int x = 0; x < mask.cols; ++x)
        {
            if (mp[x] == 0)  // 目标区域
            {
                // 混合颜色
                rp[x] = cv::Vec3b(
                    rp[x][0] * (1 - alpha) + gp[x][0] * alpha,
                    rp[x][1] * (1 - alpha) + gp[x][1] * alpha,
                    rp[x][2] * (1 - alpha) + gp[x][2] * alpha
                    );
            }
        }
    }

    return result;
}
