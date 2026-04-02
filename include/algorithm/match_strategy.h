#ifndef MATCH_STRATEGY_H
#define MATCH_STRATEGY_H

#include <QString>
#include <QVector>
#include <QPointF>
#include <QPainter>
#include <QColor>
#include <QPolygonF>
#include <QCache>
#include <QMutex>
#include "opencv2/opencv.hpp"
#include "HalconCpp.h"

using namespace cv;
using namespace HalconCpp;

// 先定义TemplateParams，因为TemplateCacheKey需要使用它
struct TemplateParams
{
    QVector<QPointF> polygonPoints;
    //shape model
    int numLevels;
    double angleStart;
    double angleExtent;
    double angleStep;
    double greediness;
    QString optimization;
    QString metric;
    //NCC
    int nccLevels;
    //opencv
    int matchMethod;
};

// 模板缓存键值生成器
struct TemplateCacheKey {
    size_t imageHash;
    QVector<QPointF> polygon;
    TemplateParams params;
    
    TemplateCacheKey(const cv::Mat& image, const QVector<QPointF>& poly, const TemplateParams& p) 
        : polygon(poly), params(p) {
        // 使用图像数据指针和尺寸生成哈希
        imageHash = std::hash<const void*>{}(image.data) ^ 
                   (image.type() << 1) ^ 
                   (image.rows << 2) ^ 
                   (image.cols << 3);
        
        // 为多边形生成哈希
        for (const QPointF& pt : poly) {
            imageHash ^= (std::hash<int>{}(static_cast<int>(pt.x())) << 4) ^
                        (std::hash<int>{}(static_cast<int>(pt.y())) << 5);
        }
        
        // 为参数生成哈希
        imageHash ^= (params.numLevels << 6) ^
                    (static_cast<int>(params.angleStart) << 7) ^
                    (static_cast<int>(params.angleExtent) << 8);
    }
    
    bool operator==(const TemplateCacheKey& other) const {
        return imageHash == other.imageHash && 
               polygon == other.polygon &&
               params.numLevels == other.params.numLevels &&
               params.angleStart == other.params.angleStart &&
               params.angleExtent == other.params.angleExtent;
    }
};

// 为TemplateCacheKey提供哈希函数
namespace std {
    template<>
    struct hash<TemplateCacheKey> {
        size_t operator()(const TemplateCacheKey& key) const {
            return key.imageHash;
        }
    };
}

struct MatchResult
{
    double row;
    double column;
    double angle;
    double score;

    QString toString() const
    {
        return QString("位置(%1, %2), 角度%3 ,分数 %4")
            .arg(column).arg(row).arg(angle).arg(score);
    }
};

class IMatchStrategy
{
public:
    virtual ~IMatchStrategy() = default;
    virtual bool createTemplate(const cv::Mat& fullImage,
                       const QVector<QPointF>& pologonPoints,
                       const TemplateParams& params) =0;
    virtual QVector<MatchResult> findMatches(const cv::Mat& searchImage,
                                     double minScore = 0.5,
                                     int maxMatches = 1,
                                     double greediness = 0.5) =0;
    virtual cv::Mat drawMatches(const cv::Mat& searchImage,
                                const QVector<MatchResult>& matches) const =0;
    virtual cv::Mat getTemplateImage() const =0 ;
    virtual QString getStrategyName() const =0;
    virtual bool hasTemplate() const= 0;
};

class ShapeMatchStrategy :public IMatchStrategy
{
public:
    ShapeMatchStrategy();
    ~ShapeMatchStrategy() override = default;

    bool createTemplate(const cv::Mat& fullImage,
                       const QVector<QPointF>& pologonPoints,
                       const TemplateParams& params) override;
    QVector<MatchResult> findMatches(const cv::Mat& searchImage,
                                     double minScore = 0.5,
                                     int maxMatches = 1,
                                     double greediness = 0.5) override;
    cv::Mat drawMatches(const cv::Mat& searchImage,
                        const QVector<MatchResult>& matches) const override;

    cv::Mat getTemplateImage() const override {return m_templateImage; }
    QString getStrategyName() const override {return "Shape Model"; }
    bool hasTemplate() const override {return m_hasTemplate;}

private:

    HShapeModel m_model;
    cv::Mat m_templateImage;
    QVector<QPointF> m_polygonPoints;
    bool m_hasTemplate;

    HRegion m_templateContour;
    HTuple m_templateRows;
    HTuple m_templateCols;
    double m_modelRow;
    double m_modelCol;

    // ROI 偏移量（用于坐标转换）
    int m_roiOffsetX = 0;
    int m_roiOffsetY = 0;
    int m_templateWidth = 0;
    int m_templateHeight = 0;

    HImage createTemplateRegion(const cv::Mat& image,
                                const QVector<QPointF>& polygon);
    void extractTemplateContour(const QVector<QPointF>& polygon);
    void drawSingleMatch(QPainter& painter,
                         const MatchResult& match,
                         const QColor& color) const;
};

// ========== Halcon NCC Model 策略 ==========
class NCCMatchStrategy : public IMatchStrategy
{
public:
    NCCMatchStrategy();
    ~NCCMatchStrategy() override = default;

    bool createTemplate(const cv::Mat& fullImage,
                        const QVector<QPointF>& pologonPoints,
                        const TemplateParams& params) override;

    QVector<MatchResult> findMatches(const cv::Mat& searchImage,
                                     double minScore,
                                     int maxMatches,
                                     double greediness) override;

    cv::Mat drawMatches(const cv::Mat& searchImage,
                        const QVector<MatchResult>& matches) const override;

    cv::Mat getTemplateImage() const override { return m_templateImage; }
    QString getStrategyName() const override { return "NCC Model"; }
    bool hasTemplate() const override { return m_hasTemplate; }

private:
    HNCCModel m_model;
    cv::Mat m_templateImage;
    QVector<QPointF> m_polygonPoints;
    bool m_hasTemplate;

    int m_templateWidth;
    int m_templateHeight;

    HImage createTemplateRegion(const cv::Mat& image,
                                const QVector<QPointF>& polygon);
};

// ========== OpenCV Template Matching 策略 ==========
class OpenCVMatchStrategy : public IMatchStrategy
{
public:
    OpenCVMatchStrategy();
    ~OpenCVMatchStrategy() override = default;

    bool createTemplate(const cv::Mat& fullImage,
                        const QVector<QPointF>& polygon,
                        const TemplateParams& params) override;

    QVector<MatchResult> findMatches(const cv::Mat& searchImage,
                                     double minScore,
                                     int maxMatches,
                                     double greediness) override;

    cv::Mat drawMatches(const cv::Mat& searchImage,
                        const QVector<MatchResult>& matches) const override;

    cv::Mat getTemplateImage() const override { return m_templateImage; }
    QString getStrategyName() const override { return "OpenCV TM"; }
    bool hasTemplate() const override { return m_hasTemplate; }

private:
    cv::Mat m_templateImage;
    bool m_hasTemplate;
    int m_matchMethod;

    QVector<QPointF> m_polygonPoints;

    cv::Mat extractTemplateROI(const cv::Mat& image,
                               const QVector<QPointF>& polygon);

    // OpenCV模板匹配不支持旋转，这里简化处理
    // 只在0度进行匹配
};

class MatchStrategy
{
public:
    MatchStrategy();
};

#endif // MATCH_STRATEGY_H
