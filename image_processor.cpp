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

// Mat imageprocessor::extractFeatures(const Mat &src)
// {

// }



