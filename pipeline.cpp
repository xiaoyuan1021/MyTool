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

cv::Mat PipelineContext::getFinalDisplay()
{
    {
        // 优先级：processed > mask > enhanced > srcBgr
        if (!processed.empty()) {
            if (processed.channels() == 1) {
                return maskToGreenWhite(processed); // 单通道转绿白显示
            }
            return processed; // 彩色直接显示
        }
        if (!mask.empty()) {
            return maskToGreenWhite(mask);
        }
        if (!enhanced.empty()) {
            if (enhanced.channels() == 1) {
                cv::Mat bgr;
                cv::cvtColor(enhanced, bgr, cv::COLOR_GRAY2BGR);
                return bgr;
            }
            return enhanced;
        }
        return srcBgr;
    }
}
