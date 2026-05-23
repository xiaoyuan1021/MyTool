#include "core/pipeline_steps.h"
#include "config/pipeline_config.h"
#include <opencv2/imgproc.hpp>

void StepImageFilter::run(PipelineContext& ctx)
{
    if (!ctx.config) return;

    const auto& cfg = ctx.config->imageFilter;

    // 获取输入图像：优先使用 enhanced，否则使用 channelImg，最后使用 srcBgr
    cv::Mat src = ctx.enhanced.empty() ? (ctx.channelImg.empty() ? ctx.srcBgr : ctx.channelImg) : ctx.enhanced;
    if (src.empty()) return;

    cv::Mat result;

    switch (cfg.filterType) {
    case FilterDenoiseType::Gaussian: {
        // 高斯滤波
        int ksize = cfg.gaussianKernelSize;
        if (ksize % 2 == 0) ksize += 1;  // 确保为奇数
        if (ksize < 1) ksize = 1;
        cv::GaussianBlur(src, result, cv::Size(ksize, ksize),
                        cfg.gaussianSigmaX, cfg.gaussianSigmaY);
        break;
    }
    case FilterDenoiseType::Median: {
        // 中值滤波
        int ksize = cfg.medianKernelSize;
        if (ksize % 2 == 0) ksize += 1;  // 确保为奇数
        if (ksize < 3) ksize = 3;
        cv::medianBlur(src, result, ksize);
        break;
    }
    case FilterDenoiseType::Bilateral: {
        // 双边滤波
        int d = cfg.bilateralD;
        if (d <= 0) d = 9;
        cv::bilateralFilter(src, result, d,
                           cfg.bilateralSigmaColor, cfg.bilateralSigmaSpace);
        break;
    }
    case FilterDenoiseType::Morphology: {
        // 形态学滤波
        int ksize = cfg.morphologyKernelSize;
        if (ksize < 1) ksize = 1;
        cv::Mat kernel = cv::getStructuringElement(cv::MORPH_ELLIPSE,
                                                   cv::Size(ksize, ksize));

        switch (cfg.morphologyOp) {
        case MorphologyOpType::Open:
            cv::morphologyEx(src, result, cv::MORPH_OPEN, kernel,
                            cv::Point(-1, -1), cfg.morphologyIterations);
            break;
        case MorphologyOpType::Close:
            cv::morphologyEx(src, result, cv::MORPH_CLOSE, kernel,
                            cv::Point(-1, -1), cfg.morphologyIterations);
            break;
        case MorphologyOpType::Erode:
            cv::erode(src, result, kernel, cv::Point(-1, -1),
                     cfg.morphologyIterations);
            break;
        case MorphologyOpType::Dilate:
            cv::dilate(src, result, kernel, cv::Point(-1, -1),
                      cfg.morphologyIterations);
            break;
        }
        break;
    }
    }

    if (!result.empty()) {
        ctx.enhanced = result;  // 将滤波结果传递给后续步骤
    }
}
