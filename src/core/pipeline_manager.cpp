#include "pipeline_manager.h"
#include "image_processor.h"
#include "barcode_step.h"
#include "config/pipeline_config_mapper.h"
#include "config/constants.h"
#include "logger.h"
#include "ui/display_renderer.h"
#include <QDebug>
#include <algorithm>

PipelineManager::PipelineManager(QObject* parent)
    : QObject(parent)
    , m_processor(std::make_unique<ImageProcessor>())
{
    PipelineConfigMapper::resetEnhancement(m_config);
    initPipeline();
}

PipelineManager::~PipelineManager() = default;

// ========== 配置管理 ==========

void PipelineManager::resetEnhancement()
{
    PipelineConfigMapper::resetEnhancement(m_config);
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

    // 标记后台Pipeline开始运行（用于reset安全）
    m_pipelineRunning.storeRelease(1);

    // 使用局部context，不共享可变状态
    PipelineContext ctx;
    ctx.srcBgr = inputImage;
    ctx.config = &config;

    try {
        m_pipeline.run(ctx);
    } catch (const std::exception& ex) {
        qDebug() << "[PipelineManager] Pipeline执行异常:" << ex.what();
        Logger::instance()->error(QString("Pipeline执行异常: %1").arg(ex.what()));
        m_pipelineRunning.storeRelease(0);
        return PipelineContext();
    } catch (...) {
        qDebug() << "[PipelineManager] Pipeline执行未知异常";
        Logger::instance()->error("Pipeline执行未知异常");
        m_pipelineRunning.storeRelease(0);
        return PipelineContext();
    }

    // 保存结果供UI线程快速读取
    {
        QMutexLocker locker(&m_contextMutex);
        m_lastContext = ctx;
    }

    m_pipelineRunning.storeRelease(0);

    // 处理延迟重置（execute执行期间UI线程请求的reset）
    if (m_hasPendingReset.loadAcquire()) {
        m_algorithmQueue.clear();
        m_config.shapeFilter.clear();
        m_displayMode = DisplayConfig::Mode::MaskGreenWhite;
        m_overlayAlpha = AppConstants::DEFAULT_OVERLAY_ALPHA;
        initPipeline();
        m_hasPendingReset.storeRelease(0);
        emit pipelineReset();
        emit algorithmQueueChanged(0);
    }

    QString message = ctx.reason.isEmpty()
                          ? "Pipeline执行完成"
                          : ctx.reason;
    emit pipelineFinished(message);

    return ctx;
}

// ========== 私有方法 ==========

void PipelineManager::initPipeline()
{
    m_pipeline = Pipeline();

    m_pipeline.add(std::make_unique<StepColorChannel>());
    m_pipeline.add(std::make_unique<StepEnhance>(m_processor.get()));
    m_pipeline.add(std::make_unique<StepGrayFilter>());
    m_pipeline.add(std::make_unique<StepColorFilter>(m_processor.get()));
    m_pipeline.add(std::make_unique<StepAlgorithmQueue>(m_processor.get(), &m_algorithmQueue));
    m_pipeline.add(std::make_unique<StepShapeFilter>());
    m_pipeline.add(std::make_unique<StepLineDetect>());
    m_pipeline.add(std::make_unique<StepReferenceLineFilter>());
    m_pipeline.add(std::make_unique<StepBarcodeRecognition>());
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

cv::Mat PipelineManager::getLastDisplayWithMode(DisplayConfig::Mode mode) const
{
    QMutexLocker locker(&m_contextMutex);
    if (m_lastContext.srcBgr.empty()) return cv::Mat();

    return DisplayRenderer::render(m_lastContext, mode);
}

bool PipelineManager::hasLastResult() const
{
    QMutexLocker locker(&m_contextMutex);
    return !m_lastContext.srcBgr.empty();
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
    m_overlayAlpha = AppConstants::DEFAULT_OVERLAY_ALPHA;
    initPipeline();

    emit pipelineReset();
    emit algorithmQueueChanged(0);
}
