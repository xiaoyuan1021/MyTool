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

    // ✅ 优化：不预先clone，因为每次循环都会重新赋值
    // 第一次循环时会创建新的Mat，之后每次都是替换
    Mat workingImage;
    HalconAlgorithm halconAlgo;

    bool firstIteration = true;

    for(const auto& step : queue)
    {
        if(!step.enabled) continue;

        if(step.type == "HalconAlgorithm")
        {
            // 确定输入：第一次用src，后续用workingImage
            const Mat& input = firstIteration ? src : workingImage;

            // 转为灰度图
            Mat gray;
            if(input.channels() == 3)
            {
                cvtColor(input, gray, COLOR_BGR2GRAY);
            }
            else
            {
                gray = input;
            }

            try
            {
                // Mat -> HRegion
                HRegion region = ImageUtils::MatToHRegion(gray);
                HRegion resultRegion = halconAlgo.execute(region, step);

                // HRegion -> Mat（这里会创建新的Mat）
                workingImage = ImageUtils::HRegionToMat(resultRegion, gray.cols, gray.rows);

                firstIteration = false;
            }
            catch (const HalconCpp::HException& ex)
            {
                qDebug() << "Halcon algorithm error:" << ex.ErrorMessage().Text();
                return firstIteration ? src : workingImage;
            }
        }
    }

    // 如果没有执行任何步骤，返回输入
    return firstIteration ? src : workingImage;
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




