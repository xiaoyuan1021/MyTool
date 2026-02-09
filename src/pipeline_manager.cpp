#include "pipeline_manager.h"
#include <QDebug>

PipelineManager::PipelineManager(QObject* parent)
    : QObject(parent)
    , m_processor(std::make_unique<imageprocessor>())
{
    m_config.resetEnhancement();
    initPipeline();
}

// ========== 配置管理 ==========

void PipelineManager::syncFromUI(QSlider* brightness, QSlider* contrast,
                                 QSlider* gamma, QSlider* sharpen,
                                 QSlider* grayLow, QSlider* grayHigh)
{
    m_config.syncConfigFromUI(brightness, contrast, gamma,
                              sharpen, grayLow, grayHigh);
}

void PipelineManager::resetEnhancement()
{
    m_config.resetEnhancement();
}

void PipelineManager::setGrayFilterEnabled(bool enabled)
{
    m_config.enableGrayFilter = enabled;
}

void PipelineManager::setAreaFilterEnabled(bool enabled)
{
    m_config.enableAreaFilter = enabled;
}

// ========== 形状筛选接口 ==========

void PipelineManager::addFilterCondition(const FilterCondition& condition)
{
    m_config.shapeFilter.addCondition(condition);
    m_config.shapeFilter.enabled = true;  // 添加条件时自动启用
}

void PipelineManager::setFilterMode(FilterMode mode)
{
    m_config.shapeFilter.mode = mode;
}

void PipelineManager::clearShapeFilter()
{
    m_config.shapeFilter.clear();
}

void PipelineManager::enableShapeFilter(bool enable)
{
    m_config.shapeFilter.enabled = enable;
}

void PipelineManager::setFeatureRange(ShapeFeature feature, double minValue, double maxValue)
{
    // 先清除旧条件（如果只想单特征筛选）
    // 或者找到同类型条件并更新

    // 简单实现：添加新条件
    FilterCondition cond(feature, minValue, maxValue);
    addFilterCondition(cond);
}

// ========== 算法队列管理 ==========

void PipelineManager::addAlgorithmStep(const AlgorithmStep& step)
{
    m_algorithmQueue.append(step);
    emit algorithmQueueChanged(m_algorithmQueue.size());
}

void PipelineManager::removeAlgorithmStep(int index)
{
    if (index >= 0 && index < m_algorithmQueue.size())
    {
        m_algorithmQueue.removeAt(index);
        emit algorithmQueueChanged(m_algorithmQueue.size());
    }
}

void PipelineManager::swapAlgorithmStep(int index1, int index2)
{
    if (index1 < m_algorithmQueue.size() && index2 < m_algorithmQueue.size())
    {
        m_algorithmQueue.swapItemsAt(index1,index2);
        emit algorithmQueueChanged(m_algorithmQueue.size());

    }
}

void PipelineManager::clearAlgorithmQueue()
{
    m_algorithmQueue.clear();
    emit algorithmQueueChanged(0);
}

// ========== Pipeline执行 ==========

const PipelineContext& PipelineManager::execute(const cv::Mat& inputImage)
{
    if (inputImage.empty())
    {
        static const PipelineContext emptyContext;
        return emptyContext;
    }

    // ✅ 显式释放所有Mat
    m_lastContext.srcBgr.release();
    m_lastContext.channelImg.release();
    m_lastContext.enhanced.release();
    m_lastContext.mask.release();
    m_lastContext.processed.release();
    m_lastContext.regions.clear();
    m_lastContext.currentRegions = 0;
    m_lastContext.pass = true;
    m_lastContext.reason.clear();

    m_lastContext.srcBgr = inputImage;

    m_lastContext.displayConfig.mode = m_displayMode;
    m_lastContext.displayConfig.overlayAlpha = m_overlayAlpha;;

    // 执行Pipeline
    m_pipeline.run(m_lastContext);

    // 发出完成信号
    QString message = m_lastContext.reason.isEmpty()
                          ? "Pipeline执行完成"
                          : m_lastContext.reason;
    emit pipelineFinished(message);

    return m_lastContext;
}

// ========== 私有方法 ==========

void PipelineManager::initPipeline()
{
    // 清空现有Pipeline
    m_pipeline = Pipeline();

    // 按顺序添加处理步骤
    m_pipeline.add(std::make_unique<StepColorChannel>(&m_config));
    m_pipeline.add(std::make_unique<StepEnhance>(&m_config, m_processor.get()));
    m_pipeline.add(std::make_unique<StepGrayFilter>(&m_config));
    m_pipeline.add(std::make_unique<StepColorFilter>(&m_config, m_processor.get()));
    m_pipeline.add(std::make_unique<StepAlgorithmQueue>(m_processor.get(), &m_algorithmQueue));
    m_pipeline.add(std::make_unique<StepShapeFilter>(&m_config));

    //qDebug() << "[PipelineManager] Pipeline初始化完成";
}

void PipelineManager::rebuildPipeline()
{
    // 当算法队列变化时，可能需要重建Pipeline
    // 目前的设计中，算法队列是引用传递，所以不需要重建
    // 但保留这个接口以备将来扩展
    qDebug() << "[PipelineManager] Pipeline重建（当前为引用传递，无需重建）";
}

void PipelineManager::setChannelMode(PipelineConfig::Channel channel)
{
    m_config.channel=channel;
}

PipelineConfig::Channel PipelineManager::getChannelMode() const
{
    return m_config.channel;
}

void PipelineManager::updateAlgorithmStep(int index, const AlgorithmStep &step)
{
    if (index >= 0 && index < m_algorithmQueue.size())
    {
        m_algorithmQueue[index] = step;
        emit algorithmQueueChanged(m_algorithmQueue.size());
    }
}

void PipelineManager::setDisplayMode(DisplayConfig::Mode mode)
{
    m_displayMode = mode;
}

void PipelineManager::setOverlayAlpha(float alpha)
{

}

void PipelineManager::setColorFilterEnabled(bool enabled)
{
    m_config.enableColorFilter = enabled;
}

void PipelineManager::setColorFilterMode(PipelineConfig::ColorFilterMode mode)
{
    m_config.colorFilterMode = mode;
}

void PipelineManager::setRGBRange(int rLow, int rHigh, int gLow, int gHigh, int bLow, int bHigh)
{
    m_config.rLow = rLow;
    m_config.rHigh = rHigh;
    m_config.gLow = gLow;
    m_config.gHigh = gHigh;
    m_config.bLow = bLow;
    m_config.bHigh = bHigh;
}

void PipelineManager::setHSVRange(int hLow, int hHigh, int sLow, int sHigh, int vLow, int vHigh)
{
    m_config.hLow = hLow;
    m_config.hHigh = hHigh;
    m_config.sLow = sLow;
    m_config.sHigh = sHigh;
    m_config.vLow = vLow;
    m_config.vHigh = vHigh;
}

void PipelineManager::setCurrentFilterMode(PipelineConfig::FilterMode mode)
{
    m_config.currentFilterMode = mode;
}

PipelineConfig::FilterMode PipelineManager::getCurrentFilterMode() const
{
    return m_config.currentFilterMode;
}

void PipelineManager::resetPipeline()
{
    //qDebug() << "[PipelineManager] ===== 开始完全重置Pipeline =====";

    // 1️⃣ 清空上次执行的上下文
    //clearLastContext();

    // 2️⃣ 重置配置到默认值
    //m_config = getDefaultConfig();
    //qDebug() << "[PipelineManager]   - 配置已重置";

    // 3️⃣ 清空算法队列
    m_algorithmQueue.clear();
    //qDebug() << "[PipelineManager]   - 算法队列已清空";

    // 4️⃣ 清空形状筛选
    m_config.shapeFilter.clear();
    //qDebug() << "[PipelineManager]   - 形状筛选已清空";

    // 5️⃣ 重置显示模式
    m_displayMode = DisplayConfig::Mode::MaskGreenWhite;
    m_overlayAlpha = 0.3f;
    //qDebug() << "[PipelineManager]   - 显示模式已重置";

    // 6️⃣ 重新初始化Pipeline
    initPipeline();
    //qDebug() << "[PipelineManager]   - Pipeline已重新初始化";

    // 7️⃣ 发出信号
    emit pipelineReset();
    emit algorithmQueueChanged(0);

    //qDebug() << "[PipelineManager] ===== Pipeline重置完成 =====";
}
