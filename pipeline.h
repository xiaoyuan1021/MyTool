#pragma once
#include <opencv2/opencv.hpp>
#include <QVector>
#include <QVariantMap>
#include <QSlider>
#include <QSpinBox>
#include "shape_filter_types.h"

struct RegionFeature
{
    double area;
    double circularity=0.0;
    cv::Rect bbox;//有啥用
};



struct PipelineConfig
{
    enum class Channel {Gray,RGB,BGR,HSV,B,G,R} channel=Channel::RGB;

    int brightness = 0;
    double contrast = 1.0;
    double gamma = 1.0;
    double sharpen=0.0;

    // 灰度过滤（选中范围）
    int grayLow = 0;
    int grayHigh = 255;
    bool enableGrayFilter = true;
    bool enableAreaFilter =false;

    // ✅ 形状筛选配置（替换原来的 minArea/maxArea）
    ShapeFilterConfig shapeFilter;
    // 判定
    // double minArea = 50.0;//最小区域
    // double maxArea = 1e18;//最大区域
    // double minCircularity = 0.0;//最小圆度
    // double maxCircularity = 1.0;//最大圆度
    // int maxRegionCount = 0; // 例如：0 表示“不能有缺陷”


    void syncConfigFromUI(QSlider* brightness,QSlider* contrast,QSlider* gamma,
                          QSlider* sharpen,QSlider* grayLow,QSlider* grayHigh)
    {
        this->brightness = brightness->value();
        this->contrast   = contrast->value() / 100.0;
        this->gamma      = gamma->value() / 100.0;
        this->sharpen    = sharpen->value() / 100.0;
        this->grayLow    = grayLow->value();
        this->grayHigh   = grayHigh->value();

        // 防呆：避免 low > high
        if (this->grayLow > this->grayHigh)
            std::swap(this->grayLow, this->grayHigh);
    }
    void resetEnhancement()
    {
        brightness=0;
        contrast=1.0;
        gamma=1.0;
        sharpen=1.0;
        enableGrayFilter=false;
    }

};

struct PipelineContext
{
    // 输入
    cv::Mat srcBgr;      // 原图(3通道)
    // 中间结果
    cv::Mat channelImg;  // 颜色通道输出（一般是灰度）
    cv::Mat enhanced;    // 增强后
    cv::Mat mask;        // 过滤得到的 mask (0/255)
    cv::Mat processed;   // 算法处理后的图（可能还是 mask / 或灰度）
    // 特征
    std::vector<RegionFeature> regions;
    // 输出

    int currentRegions=0;

    bool pass = true;
    QString reason;
    cv::Mat getFinalDisplay();
};

class IPipelineStep
{
public:
    virtual ~IPipelineStep()=default;
    virtual void run(PipelineContext& ctx)=0;

};

class Pipeline
{
public:
    Pipeline();
    void add(std::unique_ptr<IPipelineStep> step)
    {
        steps_.push_back(std::move(step));//为啥要move


    }
    void run(PipelineContext& ctx)
    {
        for(auto &s:steps_) s->run(ctx);
    }
private:
    std::vector<std::unique_ptr<IPipelineStep>> steps_;
};

cv::Mat overlayGreenMask(const cv::Mat& bgr,const cv::Mat& mask,float alpha=0.9f);
cv::Mat maskToGreenWhite(const cv::Mat& mask);

