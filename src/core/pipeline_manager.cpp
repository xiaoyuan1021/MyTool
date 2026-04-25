#include "pipeline_manager.h"
#include "barcode_step.h"
#include <QDebug>
#include <algorithm>

PipelineManager::PipelineManager(QObject* parent)
    : QObject(parent)
    , m_processor(std::make_unique<ImageProcessor>())
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
    //m_config.shapeFilter.enabled = enable;
}

void PipelineManager::setFeatureRange(ShapeFeature feature, double minValue, double maxValue)
{
    // 先清除旧条件（如果只想单特征筛选）
    // 或者找到同类型条件并更新

    // 简单实现：添加新条件
    FilterCondition cond(feature, minValue, maxValue);
    addFilterCondition(cond);
}

void PipelineManager::setShapeFilterConfig(const ShapeFilterConfig& config)
{
    m_config.shapeFilter = config;
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

PipelineContext PipelineManager::execute(const cv::Mat& inputImage, const PipelineConfig& config)
{
    if (inputImage.empty())
    {
        return PipelineContext();
    }

    // 标记后台Pipeline开始运行
    m_pipelineRunning.storeRelease(1);

    // 标记是否需要发送重置相关信号（必须在锁外发送）
    bool shouldEmitPipelineReset = false;

    // 在锁内执行所有操作：修改m_lastContext、执行Pipeline、处理pending
    {
        QMutexLocker locker(&m_configMutex);

        // 清空上次执行结果（在锁内，避免与主线程读取竞争）
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
        m_lastContext.displayConfig.overlayAlpha = m_overlayAlpha;

        // 临时设置config用于Pipeline执行
        PipelineConfig savedConfig = m_config;
        m_config = config;
        m_lastConfigSnapshot = config;

        // 执行Pipeline
        m_pipeline.run(m_lastContext);

        // 恢复m_config
        m_config = savedConfig;

        // 处理延迟的配置更新（UI线程在Pipeline执行期间设置的新配置）
        if (m_hasPendingConfig.loadAcquire()) {
            m_config = m_pendingConfig;
            m_lastConfigSnapshot = m_pendingConfig;
            m_hasPendingConfig.storeRelease(0);
        }

        // 处理延迟的Pipeline重置（信号在锁外发送）
        if (m_hasPendingReset.loadAcquire()) {
            m_algorithmQueue.clear();
            m_config.shapeFilter.clear();
            m_displayMode = DisplayConfig::Mode::MaskGreenWhite;
            m_overlayAlpha = 0.3f;
            initPipeline();
            m_hasPendingReset.storeRelease(0);
            shouldEmitPipelineReset = true;
        }
    }

    // 标记后台Pipeline运行结束
    m_pipelineRunning.storeRelease(0);

    // 发送Pipeline重置信号（必须在锁外，避免死锁）
    if (shouldEmitPipelineReset) {
        emit pipelineReset();
        emit algorithmQueueChanged(0);
    }

    // 发出完成信号（在锁外，避免死锁）
    QString message = m_lastContext.reason.isEmpty()
                          ? "Pipeline执行完成"
                          : m_lastContext.reason;
    emit pipelineFinished(message);

    // 返回副本，避免多线程竞争
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
    m_pipeline.add(std::make_unique<StepLineDetect>(&m_config));
    m_pipeline.add(std::make_unique<StepReferenceLineFilter>(&m_config));  // 添加参考线匹配步骤
    m_pipeline.add(std::make_unique<StepBarcodeRecognition>(&m_config.barcode));  // 添加条码识别步骤

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
    Q_UNUSED(alpha);
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
    // 如果后台Pipeline正在运行，标记延迟重置，等execute完成后自动执行
    if (m_pipelineRunning.loadAcquire()) {
        m_hasPendingReset.storeRelease(1);
        return;
    }

    // Pipeline未在运行，直接执行重置（无需加锁，因为在UI线程且无并发）
    m_algorithmQueue.clear();
    m_config.shapeFilter.clear();
    m_displayMode = DisplayConfig::Mode::MaskGreenWhite;
    m_overlayAlpha = 0.3f;
    initPipeline();

    emit pipelineReset();
    emit algorithmQueueChanged(0);
}
