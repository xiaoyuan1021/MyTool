#include "image_processor.h"

using namespace HalconCpp;

imageprocessor::imageprocessor() {}

Mat imageprocessor::convertColorSpace(const Mat &src, const QString &mode)
{
    if(src.empty()) return src;

    Mat displayMat;
    if(mode=="Gray Mode")
    {
        cvtColor(src,displayMat,COLOR_BGR2GRAY);
    }
    else if(mode=="HSV Mode")
    {
        cvtColor(src,displayMat,COLOR_BGR2HSV);
    }
    else if(mode=="RGB Mode")
    {
        displayMat=src;
    }
    return displayMat;
}

Mat imageprocessor::executeAlgorithmQueue(const Mat &src, const QVector<AlgorithmStep> &queue)
{
    if(src.empty()) return src;

    bool hasValidStep =false;
    for(const auto & step : queue)
    {
        if(step.enabled && step.type =="HalconAlgorithm")
        {
            hasValidStep =true;
            break;
        }
    }

    if(!hasValidStep)
    {
        qDebug() << "[executeAlgorithmQueue] 没有启用的算法步骤";
        return src;  // 没有要执行的步骤，直接返回原图
    }

    Mat gray;
    if(src.channels() == 3) {
        cvtColor(src, gray, COLOR_BGR2GRAY);
    } else if(src.channels() == 1) {
        gray = src;  // 已经是灰度图
    } else {
        qDebug() << "[executeAlgorithmQueue] 不支持的通道数:" << src.channels();
        return src;
    }

    try {
        // ✅ 优化3：在HRegion域操作，避免反复转换
        HRegion currentRegion = ImageUtils::MatToHRegion(gray);

        HalconAlgorithm halconAlgo;
        int executedSteps = 0;

        for(const auto& step : queue)
        {
            if(!step.enabled) {
                qDebug() << QString("  跳过未启用步骤: %1").arg(step.name);
                continue;
            }

            if(step.type != "HalconAlgorithm") {
                qDebug() << QString("  跳过非Halcon步骤: %1").arg(step.name);
                continue;
            }

            qDebug() << QString("  执行步骤 %1: %2")
                            .arg(executedSteps + 1)
                            .arg(step.name);

            // ✅ 直接在HRegion上链式操作（不创建临时Mat）
            currentRegion = halconAlgo.execute(currentRegion, step);
            executedSteps++;
        }

        qDebug() << QString("[executeAlgorithmQueue] 完成，共执行 %1 个步骤")
                        .arg(executedSteps);

        // ✅ 优化4：最后统一转回Mat（只转一次）
        Mat result = ImageUtils::HRegionToMat(currentRegion, gray.cols, gray.rows);

        if(result.empty()) {
            qDebug() << "[executeAlgorithmQueue] 警告：结果为空，返回输入灰度图";
            return gray;
        }

        return result;

    } catch (const HalconCpp::HException& ex) {
        qDebug() << "[executeAlgorithmQueue] Halcon异常:" << ex.ErrorMessage().Text();
        qDebug() << "  错误代码:" << ex.ErrorCode();
        return gray;  // 出错时返回灰度图
    } catch (const cv::Exception& ex) {
        qDebug() << "[executeAlgorithmQueue] OpenCV异常:" << ex.what();
        return gray;
    } catch (...) {
        qDebug() << "[executeAlgorithmQueue] 未知异常";
        return gray;
    }



}

Mat imageprocessor::adjustParameter(const Mat &src, int brightness, double contrast, double gamma,double sharpen)
{
    //CV_Assert(gamma>0);
    Mat dst;
    if(src.empty()) return src;
    src.convertTo(dst,-1,contrast,brightness);

    Mat lut(1,256,CV_8U);
    for(int i=0;i<256;i++)
        lut.at<uchar>(i)=saturate_cast<uchar>(pow(i/255.0,gamma)*255.0);
    LUT(dst,lut,dst);
    if(sharpen>0.0)
    {
        Mat blur;
        GaussianBlur(dst,blur,Size(0,0),1.0);
        // dst = dst + sharpen * (dst - blur)
        addWeighted(dst, 1.0 + sharpen,
                    blur, -sharpen,
                    0, dst);
    }
    return dst;
}

Mat imageprocessor::filterRGB(const Mat &src, int rLow, int rHigh, int gLow, int gHigh, int bLow, int bHigh)
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

Mat imageprocessor::filterHSV(const Mat &src, int hLow, int hHigh, int sLow, int sHigh, int vLow, int vHigh)
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
