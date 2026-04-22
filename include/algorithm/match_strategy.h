#ifndef MATCH_STRATEGY_H
#define MATCH_STRATEGY_H

#include <QString>
#include <QVector>
#include <QPointF>
#include <QPainter>
#include <QColor>
#include <QPolygonF>
#include "opencv2/opencv.hpp"

// 先定义TemplateParams
struct TemplateParams
{
    QVector<QPointF> polygonPoints;
    //opencv
    int matchMethod;
};

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
    virtual bool saveTemplate(const QString& imagePath, const QString& jsonPath) const = 0;
    virtual bool loadTemplate(const QString& imagePath, const QString& jsonPath) = 0;
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

    // 新增：保存和加载模板
    bool saveTemplate(const QString& imagePath, const QString& jsonPath) const;
    bool loadTemplate(const QString& imagePath, const QString& jsonPath);

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
