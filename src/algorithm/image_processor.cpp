#include "image_processor.h"
#include "opencv_algorithm.h"
#include <QDebug>

ImageProcessor::ImageProcessor() {}

cv::Mat ImageProcessor::convertColorSpace(const cv::Mat &src, const QString &mode)
{
    if(src.empty()) return src;

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
        qDebug() << "[executeAlgorithmQueue] 没有启用的算法步骤";
        return src;  // 没有要执行的步骤，直接返回原图
    }

    // 转换为灰度图
    cv::Mat gray;
    if(src.channels() == 3) {
        cv::cvtColor(src, gray, cv::COLOR_BGR2GRAY);
    } else if(src.channels() == 1) {
        gray = src.clone();  // 已经是灰度图
    } else {
        qDebug() << "[executeAlgorithmQueue] 不支持的通道数:" << src.channels();
        return src;
    }

    try {
        // ✅ 使用OpenCV算法替代Halcon
        cv::Mat currentMat = gray.clone();
        int executedSteps = 0;

        for(const auto& step : queue)
        {
            if(!step.enabled) {
                qDebug() << QString("  跳过未启用步骤: %1").arg(step.name);
                continue;
            }

            if(step.type != "OpenCVAlgorithm") {
                qDebug() << QString("  跳过非OpenCV步骤: %1").arg(step.name);
                continue;
            }

            // qDebug() << QString("  执行步骤 %1: %2")
            //                 .arg(executedSteps + 1)
            //                 .arg(step.name);

            // ✅ 直接在cv::Mat上使用OpenCV算法
            currentMat = OpenCVAlgorithm::execute(currentMat, step);
            executedSteps++;
        }

        qDebug() << QString("[executeAlgorithmQueue] 完成，共执行 %1 个步骤")
                        .arg(executedSteps);

        if(currentMat.empty()) {
            qDebug() << "[executeAlgorithmQueue] 警告：结果为空，返回输入灰度图";
            return gray;
        }

        return currentMat;

    } catch (const cv::Exception& ex) {
        qDebug() << "[executeAlgorithmQueue] OpenCV异常:" << ex.what();
        return gray;
    } catch (...) {
        qDebug() << "[executeAlgorithmQueue] 未知异常";
        return gray;
    }
}

cv::Mat ImageProcessor::adjustParameter(const cv::Mat &src, int brightness, double contrast, double gamma,double sharpen)
{
    //CV_Assert(gamma>0);
    cv::Mat dst;
    if(src.empty()) return src;
    src.convertTo(dst,-1,contrast,brightness);

    cv::Mat lut(1,256,CV_8U);
    for(int i=0;i<256;i++)
        lut.at<uchar>(i)=cv::saturate_cast<uchar>(pow(i/255.0,gamma)*255.0);
    cv::LUT(dst,lut,dst);
    if(sharpen>0.0)
    {
        cv::Mat blur;
        cv::GaussianBlur(dst,blur,cv::Size(0,0),1.0);
        // dst = dst + sharpen * (dst - blur)
        cv::addWeighted(dst, 1.0 + sharpen,
                    blur, -sharpen,
                    0, dst);
    }
    return dst;
}

cv::Mat ImageProcessor::filterRGB(const cv::Mat &src, int rLow, int rHigh, int gLow, int gHigh, int bLow, int bHigh)
{
    if(src.empty())
    {
        qDebug()<<"[filterRGB] 输入图像为空";
        return cv::Mat();
    }
    cv::Mat bgr;
    if (src.channels() == 1) {
        // 灰度图 → BGR（三个通道都是相同值）
        cv::cvtColor(src, bgr, cv::COLOR_GRAY2BGR);
    } else if (src.channels() == 3) {
        bgr = src;
    } else {
        qDebug() << "[filterRGB] 不支持的通道数:" << src.channels();
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
}

cv::Mat ImageProcessor::filterHSV(const cv::Mat &src, int hLow, int hHigh, int sLow, int sHigh, int vLow, int vHigh)
{
    if(src.empty())
    {
        qDebug()<<"[filterRGB] 输入图像为空";
        return cv::Mat();
    }

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
        qDebug() << "[filterHSV] 不支持的通道数:" << src.channels();
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
}
