#ifndef TEMPLATE_MATCH_MANAGER_H
#define TEMPLATE_MATCH_MANAGER_H

#include <QObject>
#include <QVector>
#include <QPointF>
#include <QString>
#include <opencv2/opencv.hpp>
#include "halconcpp/HalconCpp.h"
#include "image_utils.h"

using namespace HalconCpp;

// ========== 模板数据结构 ==========
struct TemplateData
{
    QString name;                    // 模板名称
    HShapeModel model;               // Halcon 模板对象
    cv::Mat templateImage;           // 模板图像（用于显示）
    QVector<QPointF> polygonPoints;  // 绘制的多边形顶点

    // 创建参数
    int numLevels;
    double angleStart;
    double angleExtent;
    double angleStep;
    QString optimization;
    QString metric;
    int contrast;
    int minContrast;

    HRegion templateContour;
    HTuple templateRows;
    HTuple templateCols;

    TemplateData()
        : numLevels(0)
        , angleStart(-10.0)
        , angleExtent(20.0)
        , angleStep(1.0)
        , optimization("auto")
        , metric("use_polarity")
        , contrast(30)
        , minContrast(10)
    {}
};

// ========== 匹配结果结构 ==========
struct MatchResult
{
    double row;          // 中心Y坐标
    double column;       // 中心X坐标
    double angle;        // 旋转角度
    double score;        // 匹配分数 [0-1]

    QString toString() const {
        return QString("位置: (%1, %2), 角度: %3, 分数: %4 ")
            .arg(column).arg(row).arg(angle).arg(score);
    }
};

// ========== 模板匹配管理器 ==========
class TemplateMatchManager : public QObject
{
    Q_OBJECT

public:
    explicit TemplateMatchManager(QObject* parent = nullptr);
    ~TemplateMatchManager();

    // ========== 模板创建 ==========
    bool createTemplate(const QString& name,
                        const cv::Mat& fullImage,
                        const QVector<QPointF>& polygon,
                        const TemplateData& params);

    // ========== 模板匹配 ==========
    QVector<MatchResult> findTemplate(const cv::Mat& searchImage,
                                      int templateIndex = 0,
                                      double minScore = 0.5,
                                      int maxMatches = 1,
                                      double greediness = 0.5);
    cv::Mat drawMatches(const cv::Mat& searchImage, int templateIndex,
                        const QVector<MatchResult>& matches )const;

    // ========== 模板管理 ==========
    int getTemplateCount() const { return m_templates.size(); }
    QStringList getTemplateNames() const;
    TemplateData getTemplate(int index) const;
    cv::Mat getTemplateImage(int index) const;
    bool removeTemplate(int index);
    void clearAllTemplates();

    // ========== 参数设置 ==========
    void setDefaultParams(const TemplateData& params);
    TemplateData getDefaultParams() const { return m_defaultParams; }

signals:
    void templateCreated(const QString& name);
    void templateRemoved(int index);
    void matchCompleted(int matchCount);

private:
    QVector<TemplateData> m_templates;
    TemplateData m_defaultParams;

    // 辅助函数
    HImage createTemplateRegion(const cv::Mat& image,
                                const QVector<QPointF>& polygon);
    void extractTemplateContour(TemplateData & templateData, const QVector<QPointF>& polygon);
    void drawSingleMatch(cv::Mat& image, const TemplateData& templateData,
                         const MatchResult& match, const cv::Scalar& color) const;
};

#endif // TEMPLATE_MATCH_MANAGER_H
