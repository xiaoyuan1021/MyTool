#include "image_processor.h"
#include "opencv_algorithm.h"
#include "logger.h"

ImageProcessor::ImageProcessor() {}

cv::Mat ImageProcessor::convertColorSpace(const cv::Mat &src, const QString &mode)
{
    if(src.empty()) return src;

    try {
        cv::Mat displayMat;
        if(mode=="Gray Mode")
        {
            cv::cvtColor(src,displayMat,cv::COLOR_BGR2GRAY);
        }
        else if(mode=="HSV Mode")
        {
            cv::cvtColor(src,displayMat,cv::COLOR_BGR2HSV);
        }
        else if(mode=="RGB Mode")
        {
            displayMat=src;
        }
        return displayMat;
    } catch (const cv::Exception& ex) {
        spdlog::error("颜色空间转换错误: {}", ex.what());
        return src;
    } catch (...) {
        spdlog::info("[convertColorSpace] 未知异常");
        spdlog::error("颜色空间转换未知异常");
        return src;
    }
}

cv::Mat ImageProcessor::executeAlgorithmQueue(const cv::Mat &src, const QVector<AlgorithmStep> &queue)
{
    if(src.empty()) return src;

    // 检查是否有有效的算法步骤
    bool hasValidStep = false;
    for(const auto & step : queue)
    {
        if(step.enabled && step.type == "OpenCVAlgorithm")
        {
            hasValidStep = true;
            break;
        }
    }

    if(!hasValidStep)
    {
        spdlog::info("[executeAlgorithmQueue] 没有启用的算法步骤");
        return src;  // 没有要执行的步骤，直接返回原图
    }

    // 转换为灰度图
    cv::Mat gray;
    if(src.channels() == 3) {
        cv::cvtColor(src, gray, cv::COLOR_BGR2GRAY);
    } else if(src.channels() == 1) {
        gray = src.clone();  // 已经是灰度图
    } else {
        spdlog::debug("[executeAlgorithmQueue] 不支持的通道数: {}", src.channels());
        return src;
    }

    try {
        cv::Mat currentMat = gray.clone();
        int executedSteps = 0;

        for(const auto& step : queue)
        {
            if(!step.enabled) {
                spdlog::info("  跳过未启用步骤: {}", step.name.toStdString());
                continue;
            }

            if(step.type != "OpenCVAlgorithm") {
                spdlog::info("  跳过非OpenCV步骤: {}", step.name.toStdString());
                continue;
            }

            // 直接在cv::Mat上使用OpenCV算法
            currentMat = OpenCVAlgorithm::execute(currentMat, step);
            executedSteps++;
        }

        spdlog::info("[executeAlgorithmQueue] 完成，共执行 {} 个步骤", executedSteps);

        if(currentMat.empty()) {
            spdlog::info("[executeAlgorithmQueue] 警告：结果为空，返回输入灰度图");
            return gray;
        }

        return currentMat;

    } catch (const cv::Exception& ex) {
        spdlog::debug("[executeAlgorithmQueue] OpenCV异常: {}", ex.what());
        return gray;
    } catch (...) {
        spdlog::info("[executeAlgorithmQueue] 未知异常");
        return gray;
    }
}

cv::Mat ImageProcessor::adjustParameter(const cv::Mat &src, int brightness, double contrast, double gamma,double sharpen)
{
    if(src.empty()) return src;

    try {
        cv::Mat dst;
        src.convertTo(dst,-1,contrast,brightness);

        cv::Mat lut(1,256,CV_8U);
        for(int i=0;i<256;i++)
            lut.at<uchar>(i)=cv::saturate_cast<uchar>(pow(i/255.0,gamma)*255.0);
        cv::LUT(dst,lut,dst);
        if(sharpen>0.0)
        {
            cv::Mat blur;
            cv::GaussianBlur(dst,blur,cv::Size(0,0),1.0);
            cv::addWeighted(dst, 1.0 + sharpen,
                        blur, -sharpen,
                        0, dst);
        }
        return dst;
    } catch (const cv::Exception& ex) {
        spdlog::error("图像增强参数调整错误: {}", ex.what());
        return src;
    } catch (...) {
        spdlog::info("[adjustParameter] 未知异常");
        spdlog::error("图像增强参数调整未知异常");
        return src;
    }
}

cv::Mat ImageProcessor::filterRGB(const cv::Mat &src, int rLow, int rHigh, int gLow, int gHigh, int bLow, int bHigh)
{
    if(src.empty())
    {
        spdlog::info("[filterRGB] 输入图像为空");
        return cv::Mat();
    }

    try {
        cv::Mat bgr;
        if (src.channels() == 1) {
            cv::cvtColor(src, bgr, cv::COLOR_GRAY2BGR);
        } else if (src.channels() == 3) {
            bgr = src;
        } else {
            spdlog::debug("[filterRGB] 不支持的通道数: {}", src.channels());
            return cv::Mat();
        }

        rLow=std::clamp(rLow,0,255);
        rHigh=std::clamp(rHigh,rLow,255);
        gLow=std::clamp(gLow,0,255);
        gHigh=std::clamp(gHigh,gLow,255);
        bLow=std::clamp(bLow,0,255);
        bHigh=std::clamp(bHigh,bLow,255);

        cv::Mat mask;
        cv::Scalar lower(bLow,gLow,rLow);
        cv::Scalar upper(bHigh,gHigh,rHigh);

        cv::inRange(bgr,lower,upper,mask);

        return mask;
    } catch (const cv::Exception& ex) {
        spdlog::error("RGB过滤错误: {}", ex.what());
        return cv::Mat();
    } catch (...) {
        spdlog::info("[filterRGB] 未知异常");
        spdlog::error("RGB过滤未知异常");
        return cv::Mat();
    }
}

cv::Mat ImageProcessor::filterHSV(const cv::Mat &src, int hLow, int hHigh, int sLow, int sHigh, int vLow, int vHigh)
{
    if(src.empty())
    {
        spdlog::info("[filterHSV] 输入图像为空");
        return cv::Mat();
    }

    try {
        cv::Mat hsv;
        if(src.channels()==3)
        {
            cv::cvtColor(src,hsv, cv::COLOR_BGR2HSV);
        }
        else if(src.channels()==1)
        {
            cv::Mat bgr;
            cv::cvtColor(src,bgr,cv::COLOR_GRAY2BGR);
            cv::cvtColor(bgr,hsv,cv::COLOR_BGR2HSV);
        }
        else
        {
            spdlog::debug("[filterHSV] 不支持的通道数: {}", src.channels());
            return cv::Mat();
        }

        hLow = std::clamp(hLow, 0, 179);
        hHigh = std::clamp(hHigh, hLow, 179);
        sLow = std::clamp(sLow, 0, 255);
        sHigh = std::clamp(sHigh, sLow, 255);
        vLow = std::clamp(vLow, 0, 255);
        vHigh = std::clamp(vHigh, vLow, 255);

        cv::Mat mask;
        cv::Scalar lower(hLow,sLow,vLow);
        cv::Scalar upper(hHigh,sHigh,vHigh);

        cv::inRange(hsv,lower,upper,mask);
        return mask;
    } catch (const cv::Exception& ex) {
        spdlog::error("HSV过滤错误: {}", ex.what());
        return cv::Mat();
    } catch (...) {
        spdlog::info("[filterHSV] 未知异常");
        spdlog::error("HSV过滤未知异常");
        return cv::Mat();
    }
}
