#ifndef TEMPLATE_MATCH_MANAGER_H
#define TEMPLATE_MATCH_MANAGER_H

#include <QObject>
#include <QVector>
#include <QPointF>
#include <QString>
#include <opencv2/opencv.hpp>
#include "halconcpp/HalconCpp.h"
#include "image_utils.h"
#include "match_strategy.h"
using namespace HalconCpp;

// ========== 匹配类型枚举 ==========
enum class MatchType {
    ShapeModel,    // Halcon Shape Model
    NCCModel,      // Halcon NCC Model
    OpenCVTM       // OpenCV Template Matching
};

// ========== 模板数据结构 ==========
// struct TemplateData
// {
//     QString name;                    // 模板名称
//     HShapeModel model;               // Halcon 模板对象
//     cv::Mat templateImage;           // 模板图像（用于显示）
//     QVector<QPointF> polygonPoints;  // 绘制的多边形顶点

//     // 创建参数
//     int numLevels;
//     double angleStart;
//     double angleExtent;
//     double angleStep;
//     QString optimization;
//     QString metric;
//     int contrast;
//     int minContrast;

//     HRegion templateContour;
//     HTuple templateRows;
//     HTuple templateCols;

//     TemplateData()
//         : numLevels(0)
//         , angleStart(-10.0)
//         , angleExtent(20.0)
//         , angleStep(1.0)
//         , optimization("auto")
//         , metric("use_polarity")
//         , contrast(30)
//         , minContrast(10)
//     {}
// };

// ========== 匹配结果结构 ==========
// struct MatchResult
// {
//     double row;          // 中心Y坐标
//     double column;       // 中心X坐标
//     double angle;        // 旋转角度
//     double score;        // 匹配分数 [0-1]

//     QString toString() const {
//         return QString("位置: (%1, %2), 角度: %3, 分数: %4 ")
//             .arg(column).arg(row).arg(angle).arg(score);
//     }
// };

// ========== 模板匹配管理器 ==========
class TemplateMatchManager : public QObject
{
    Q_OBJECT

public:
    explicit TemplateMatchManager(QObject* parent = nullptr);
    ~TemplateMatchManager();

    // ========== 策略选择 ==========
    void setMatchType(MatchType type);
    MatchType getCurrentMatchType() const { return m_currentType; }
    QString getCurrentStrategyName() const;

    // ========== 模板创建 ==========
    bool createTemplate(const QString& name,
                        const cv::Mat& fullImage,
                        const QVector<QPointF>& polygon,
                        const TemplateParams& params);

    // ========== 模板匹配 ==========
    QVector<MatchResult> findTemplate(const cv::Mat& searchImage,
                                      double minScore = 0.5,
                                      int maxMatches = 1,
                                      double greediness = 0.5);

    cv::Mat drawMatches(const cv::Mat& searchImage,
                        const QVector<MatchResult>& matches) const;

    // ========== 模板管理 ==========
    bool hasTemplate() const;
    cv::Mat getTemplateImage() const;
    void clearTemplate();

    // ========== 参数设置 ==========
    void setDefaultParams(const TemplateParams& params);
    TemplateParams getDefaultParams() const { return m_defaultParams; }

    // ========== 辅助方法 ==========
    static QStringList getAvailableMatchTypes();
    static MatchType matchTypeFromString(const QString& typeName);
    static QString matchTypeToString(MatchType type);

signals:
    void templateCreated(const QString& name, MatchType type);
    void templateCleared();
    void matchCompleted(int matchCount);
    void strategyChanged(MatchType newType);

private:
    // 当前策略
    std::shared_ptr<IMatchStrategy> m_currentStrategy;
    MatchType m_currentType;

    // 所有可用的策略（单例模式，避免重复创建）
    QMap<MatchType, std::shared_ptr<IMatchStrategy>> m_strategies;

    // 默认参数
    TemplateParams m_defaultParams;

    // 初始化所有策略
    void initializeStrategies();
};

#endif // TEMPLATE_MATCH_MANAGER_H
