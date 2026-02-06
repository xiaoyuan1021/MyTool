#include "template_match_manager.h"
#include "logger.h"
#include <QDebug>

TemplateMatchManager::TemplateMatchManager(QObject* parent)
    : QObject(parent)
    , m_currentType(MatchType::ShapeModel)
{
    // 设置默认参数
    m_defaultParams.numLevels = 0;        // auto
    m_defaultParams.angleStart = -10.0;   // -10度
    m_defaultParams.angleExtent = 20.0;   // 范围20度
    m_defaultParams.angleStep = 1.0;      // 步长1度
    m_defaultParams.optimization = "auto";
    m_defaultParams.metric = "use_polarity";
    m_defaultParams.nccLevels = 0;
    m_defaultParams.matchMethod = cv::TM_CCOEFF_NORMED;

    // 初始化所有策略
    initializeStrategies();
}

TemplateMatchManager::~TemplateMatchManager()
{
    clearTemplate();
}

// ========== 初始化策略 ==========
void TemplateMatchManager::initializeStrategies()
{
    // 创建所有策略实例（只创建一次）
    m_strategies[MatchType::ShapeModel] = std::make_shared<ShapeMatchStrategy>();
    m_strategies[MatchType::NCCModel] = std::make_shared<NCCMatchStrategy>();
    m_strategies[MatchType::OpenCVTM] = std::make_shared<OpenCVMatchStrategy>();

    // 设置默认策略
    m_currentStrategy = m_strategies[MatchType::ShapeModel];

    Logger::instance()->info("✅ 模板匹配管理器初始化完成，支持3种匹配算法");
}

// ========== 策略选择 ==========
void TemplateMatchManager::setMatchType(MatchType type)
{
    if (type == m_currentType) {
        return;  // 无需切换
    }

    if (!m_strategies.contains(type)) {
        Logger::instance()->error(
            QString("切换策略失败：不支持的匹配类型 %1").arg(static_cast<int>(type))
            );
        return;
    }

    // 切换策略
    m_currentType = type;
    m_currentStrategy = m_strategies[type];

    Logger::instance()->info(
        QString("已切换到: %1").arg(m_currentStrategy->getStrategyName())
        );

    emit strategyChanged(type);
}

QString TemplateMatchManager::getCurrentStrategyName() const
{
    if (m_currentStrategy) {
        return m_currentStrategy->getStrategyName();
    }
    return "未知策略";
}

// ========== 模板创建 ==========
bool TemplateMatchManager::createTemplate(const QString& name,
                                          const cv::Mat& fullImage,
                                          const QVector<QPointF>& polygon,
                                          const TemplateParams& params)
{
    if (!m_currentStrategy) {
        Logger::instance()->error("创建模板失败：未选择匹配策略");
        return false;
    }

    // 使用当前策略创建模板
    bool success = m_currentStrategy->createTemplate(fullImage, polygon, params);

    if (success) {
        emit templateCreated(name, m_currentType);
    }

    return success;
}

// ========== 模板匹配 ==========
QVector<MatchResult> TemplateMatchManager::findTemplate(const cv::Mat& searchImage,
                                                        double minScore,
                                                        int maxMatches,
                                                        double greediness)
{
    QVector<MatchResult> results;

    if (!m_currentStrategy) {
        Logger::instance()->error("匹配失败：未选择匹配策略");
        return results;
    }

    if (!m_currentStrategy->hasTemplate()) {
        Logger::instance()->error("匹配失败：当前策略未创建模板");
        return results;
    }

    // 使用当前策略执行匹配
    results = m_currentStrategy->findMatches(searchImage, minScore, maxMatches, greediness);

    emit matchCompleted(results.size());

    return results;
}

cv::Mat TemplateMatchManager::drawMatches(const cv::Mat& searchImage,
                                          const QVector<MatchResult>& matches) const
{
    if (!m_currentStrategy) {
        Logger::instance()->error("绘制失败：未选择匹配策略");
        return searchImage.clone();
    }

    return m_currentStrategy->drawMatches(searchImage, matches);
}

// ========== 模板管理 ==========
bool TemplateMatchManager::hasTemplate() const
{
    if (!m_currentStrategy) {
        return false;
    }
    return m_currentStrategy->hasTemplate();
}

cv::Mat TemplateMatchManager::getTemplateImage() const
{
    if (!m_currentStrategy) {
        return cv::Mat();
    }
    return m_currentStrategy->getTemplateImage();
}

void TemplateMatchManager::clearTemplate()
{
    // 清空所有策略的模板（重新创建策略实例）
    for (auto& pair : m_strategies.toStdMap()) {
        MatchType type = pair.first;
        switch (type) {
        case MatchType::ShapeModel:
            m_strategies[type] = std::make_shared<ShapeMatchStrategy>();
            break;
        case MatchType::NCCModel:
            m_strategies[type] = std::make_shared<NCCMatchStrategy>();
            break;
        case MatchType::OpenCVTM:
            m_strategies[type] = std::make_shared<OpenCVMatchStrategy>();
            break;
        }
    }

    // 更新当前策略指针
    m_currentStrategy = m_strategies[m_currentType];

    Logger::instance()->info("已清空所有模板");
    emit templateCleared();
}

// ========== 参数设置 ==========
void TemplateMatchManager::setDefaultParams(const TemplateParams& params)
{
    m_defaultParams = params;
}

// ========== 辅助方法 ==========
QStringList TemplateMatchManager::getAvailableMatchTypes()
{
    return QStringList{
        "Halcon Shape Model",
        "Halcon NCC Model",
        "OpenCV Template Matching"
    };
}

MatchType TemplateMatchManager::matchTypeFromString(const QString& typeName)
{
    if (typeName == "ShapeModel") {
        return MatchType::ShapeModel;
    } else if (typeName == "NCC Model") {
        return MatchType::NCCModel;
    } else if (typeName == "Opencv Model") {
        return MatchType::OpenCVTM;
    }
    return MatchType::ShapeModel;  // 默认
}

QString TemplateMatchManager::matchTypeToString(MatchType type)
{
    switch (type) {
    case MatchType::ShapeModel:
        return "Halcon Shape Model";
    case MatchType::NCCModel:
        return "Halcon NCC Model";
    case MatchType::OpenCVTM:
        return "OpenCV Template Matching";
    default:
        return "Unknown";
    }
}
