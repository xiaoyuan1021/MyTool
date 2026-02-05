#ifndef MATCH_STRATEGY_H
#define MATCH_STRATEGY_H

#include <QString>
#include <QVector>
#include <QPointF>
#include "opencv2/opencv.hpp"
#include "HalconCpp.h"

using namespace cv;
using namespace HalconCpp;

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

struct TemplateParams
{
    QVector<QPointF> polygonPoints;
    //shape model
    int numLevels;
    double angleStart;
    double angleExtent;
    double angleStep;
    QString optimization;
    QString metric;
    //NCC
    int nccLevels;
    //opencv
    int matchMethod;
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

    HImage createTemplateRegion(const cv::Mat& image,
                                const QVector<QPointF>& polygon);
    void extractTemplateContour(const QVector<QPointF>& polygon);
    void drawSingleMatch(cv::Mat& image,
                         const MatchResult& match,
                         const cv::Scalar& color) const;
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

class match_strategy
{
public:
    match_strategy();
};

#endif // MATCH_STRATEGY_H
