#include "pipeline.h"

// ========== Pipeline类实现 ==========

Pipeline::Pipeline() 
{
    //qDebug() << "[Pipeline] Pipeline实例创建";
}

void Pipeline::add(std::unique_ptr<IPipelineStep> step)
{
    if (step) {
        steps_.push_back(std::move(step));
        //qDebug() << "[Pipeline] 添加步骤，当前步骤数:" << steps_.size();
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
            //qDebug() << "[Pipeline] 执行步骤" << (i + 1);
            step->run(ctx);
        } else {
            qDebug() << "[Pipeline] 警告：步骤" << (i + 1) << "为空，跳过";
        }
    }
    
    //qDebug() << "[Pipeline] 执行完成";
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
        // 优先显示 mask（过滤结果），其次 processed
        // 灰度过滤等场景下mask是主要结果，应该优先显示
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
}


cv::Mat PipelineContext::overlayMaskOnImage(const cv::Mat &bgr, const cv::Mat &mask) const
{
    if (bgr.empty() || mask.empty()) return bgr;

    // 确保尺寸匹配
    if (bgr.size() != mask.size()) {
        qDebug() << "[overlayMaskOnImage] 尺寸不匹配";
        return bgr;
    }

    // 确保 mask 为 CV_8U 类型
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
            if (mp[x] == 0)  // 目标区域 - 设为纯绿色
            {
                rp[x] = cv::Vec3b(0, 255, 0);  // 纯绿色
            }
            // else: 背景区域保持原图不变
        }
    }

    return result;
}
