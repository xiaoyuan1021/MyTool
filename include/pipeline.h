#pragma once
#include <opencv2/opencv.hpp>
#include <QVector>
#include <QVariantMap>
#include <QSlider>
#include <QSpinBox>
#include "shape_filter_types.h"

struct RegionFeature
{
    int index = 0;           // 区域索引
    double area = 0.0;      // 面积
    double circularity = 0.0;  // 圆度
    double centerX = 0.0;    //中心X坐标
    double centerY = 0.0;   // 中心Y坐标
    double width = 0.0;     // 宽度
    double height = 0.0;    // 高度
    cv::Rect bbox;          // 外接矩形（保留原有的）

    // 转字符串方法
    QString toString() const
    {
        return QString::asprintf(
            "区域 %d: 面积=%.1f, 圆度=%.3f, 中心=(%.1f,%.1f), 尺寸=%.1f×%.1f",
            index, area, circularity, centerX, centerY, width, height
            );
    }
};

struct DisplayConfig
{
    enum class Mode
    {
        Original,           // 显示原图
        Enhanced,          // 显示增强后的图
        MaskGreenWhite,    // Mask 显示为绿白
        MaskOverlay,       // Mask 叠加在原图上
        Processed          // 显示算法处理结果
    };

    Mode mode=Mode::Original;
    float overlayAlpha =0.3f;

    bool shouldShowGreenWhite() const
    {
        return mode == Mode::MaskGreenWhite;
    }

    bool shouldOverlay() const
    {
        return mode == Mode::MaskOverlay;
    }
};

struct PipelineConfig
{
    enum class Channel {Gray,RGB,BGR,HSV,B,G,R} channel=Channel::RGB;

    enum class ColorFilterMode { None, RGB, HSV };

    // ========== 过滤模式枚举 ==========
    enum class FilterMode {
        None,      // 无过滤，只显示增强后的图像
        Gray,      // 灰度过滤模式
        RGB,       // RGB 颜色过滤模式
        HSV        // HSV 颜色过滤模式
    };

    FilterMode currentFilterMode = FilterMode::None;  // ✅ 新增：当前过滤模式

    bool enableColorFilter = false;
    ColorFilterMode colorFilterMode = ColorFilterMode::None;

    int brightness = 0;
    double contrast = 1.0;
    double gamma = 1.0;
    double sharpen=0.0;

    // 灰度过滤（选中范围）
    int grayLow = 0;
    int grayHigh = 255;
    bool enableGrayFilter = true;
    bool enableAreaFilter =false;


    // RGB 过滤范围
    int rLow = 0, rHigh = 255;
    int gLow = 0, gHigh = 255;
    int bLow = 0, bHigh = 255;

    // HSV 过滤范围
    int hLow = 0, hHigh = 179;
    int sLow = 0, sHigh = 255;
    int vLow = 0, vHigh = 255;

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
    DisplayConfig displayConfig;
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

    cv::Mat getFinalDisplay() const;

private:
    cv::Mat convertToGreenWhite(const cv::Mat& mask) const;
    cv::Mat overlayMaskOnImage(const cv::Mat& bgr,const cv::Mat& mask) const;
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
