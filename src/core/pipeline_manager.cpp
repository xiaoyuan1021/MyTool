#include "pipeline_manager.h"
#include "image_processor.h"
#include "barcode_step.h"
#include "ocr_step.h"
#include "config/constants.h"
#include "logger.h"
#include "algorithm/display_renderer.h"
#include "utils/benchmark.h"
#include <algorithm>
#include <map>

PipelineManager::PipelineManager(QObject* parent)
    : QObject(parent)
    , m_processor(std::make_unique<ImageProcessor>())
    , m_scheduler(std::make_unique<PipelineScheduler>(this))
{
    resetConfigToDefaults();
    initPipeline();

    // 连接调度器信号
    connect(m_scheduler.get(), &PipelineScheduler::finished,
            this, &PipelineManager::asyncFinished);
}

PipelineManager::~PipelineManager() = default;

// ========== 配置管理 ==========

void PipelineManager::resetConfigToDefaults()
{
    m_config.resetToDefaults();
    spdlog::info("[PipelineManager] resetConfigToDefaults");
}

// ========== 算法队列管理 ==========

void PipelineManager::addAlgorithmStep(const AlgorithmStep& step)
{
    m_algorithmQueue.append(step);
    m_config.algorithmQueue = m_algorithmQueue;
    emit algorithmQueueChanged(m_algorithmQueue.size());
}

void PipelineManager::removeAlgorithmStep(int index)
{
    if (index >= 0 && index < m_algorithmQueue.size())
    {
        m_algorithmQueue.removeAt(index);
        m_config.algorithmQueue = m_algorithmQueue;
        emit algorithmQueueChanged(m_algorithmQueue.size());
    }
}

void PipelineManager::swapAlgorithmStep(int index1, int index2)
{
    if (index1 < m_algorithmQueue.size() && index2 < m_algorithmQueue.size())
    {
        m_algorithmQueue.swapItemsAt(index1, index2);
        m_config.algorithmQueue = m_algorithmQueue;
        emit algorithmQueueChanged(m_algorithmQueue.size());
    }
}

void PipelineManager::clearAlgorithmQueue()
{
    m_algorithmQueue.clear();
    m_config.algorithmQueue.clear();
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

    // 【关键修复】保存共享状态，每个ROI执行后恢复，防止跨ROI污染
    QVector<AlgorithmStep> savedAlgorithmQueue = m_algorithmQueue;
    ShapeFilterConfig savedShapeFilter = m_config.shapeFilter;

    // 使用ROI专属的算法队列和形状滤波配置
    m_algorithmQueue = config.algorithmQueue;
    m_config.shapeFilter = config.shapeFilter;

    // 使用局部context，不共享可变状态
    PipelineContext ctx;
    ctx.srcBgr = inputImage;
    ctx.visualBase = inputImage;  // 初始化可视化基底为原图
    ctx.config = &config;

    try {
        {
            BenchmarkTimer t("Pipeline::run", &m_lastExecMs);
            m_pipeline.run(ctx);
        }
    } catch (const std::exception& ex) {
spdlog::error(QString("Pipeline执行异常: %1").arg(ex.what()));
        m_algorithmQueue = savedAlgorithmQueue;
        m_config.shapeFilter = savedShapeFilter;
        m_pipelineRunning.storeRelease(0);
        return PipelineContext();
    } catch (...) {
        spdlog::info("[PipelineManager] Pipeline执行未知异常");
        spdlog::error("Pipeline执行未知异常");
        m_algorithmQueue = savedAlgorithmQueue;
        m_config.shapeFilter = savedShapeFilter;
        m_pipelineRunning.storeRelease(0);
        return PipelineContext();
    }

    // 保存结果供UI线程快速读取
    {
        QMutexLocker locker(&m_contextMutex);
        m_lastContext = ctx;
    }

    // 【关键修复】恢复共享状态，让UI线程看到原始配置
    m_algorithmQueue = savedAlgorithmQueue;
    m_config.shapeFilter = savedShapeFilter;

    m_pipelineRunning.storeRelease(0);

    // 处理延迟重置（execute执行期间UI线程请求的reset）
    if (m_hasPendingReset.loadAcquire()) {
        m_algorithmQueue.clear();
        m_config.algorithmQueue.clear();
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

// ========== 调度器接口 ==========

qint64 PipelineManager::executeAsync(const cv::Mat& image, const PipelineConfig& config, int priority)
{
    return m_scheduler->submit(image, config, priority);
}

// ========== 非trivial方法 ==========

void PipelineManager::updateAlgorithmStep(int index, const AlgorithmStep& step)
{
    if (index >= 0 && index < m_algorithmQueue.size())
    {
        m_algorithmQueue[index] = step;
        m_config.algorithmQueue = m_algorithmQueue;
        emit algorithmQueueChanged(m_algorithmQueue.size());
    }
}

void PipelineManager::clearLastResult()
{
    QMutexLocker locker(&m_contextMutex);
    m_lastContext = PipelineContext();
}

void PipelineManager::setDisplayMode(DisplayConfig::Mode mode)
{
    m_displayMode = mode;
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
    m_config.algorithmQueue.clear();
    m_config.shapeFilter.clear();
    // [FIX] 重置步骤配置到默认值
    m_config.stepEnabled = {false, false, false, false, false, false, false, false, false};
    m_config.stepOrder = {0, 1, 2, 3, 4, 5, 6, 7, 8};
    m_config.enableObjectDetection = false;
    m_displayMode = DisplayConfig::Mode::MaskGreenWhite;
    m_overlayAlpha = AppConstants::DEFAULT_OVERLAY_ALPHA;
    initPipeline();

    emit pipelineReset();
    emit algorithmQueueChanged(0);
}

void PipelineManager::setStepEnabled(int stepIndex, bool enabled)
{
    if (stepIndex < 0 || stepIndex >= PipelineConfig::STEP_COUNT) return;
    m_config.stepEnabled[stepIndex] = enabled;
}

bool PipelineManager::isStepEnabled(int stepIndex) const
{
    if (stepIndex < 0 || stepIndex >= PipelineConfig::STEP_COUNT) return false;
    return m_config.stepEnabled[stepIndex];
}

void PipelineManager::rebuildPipeline()
{
    if (m_pipelineRunning.loadAcquire()) {
        m_hasPendingReset.storeRelease(1);
        return;
    }
    initPipeline();
    emit pipelineReset();
}

// ========== 私有方法 ==========

void PipelineManager::initPipeline()
{
    m_pipeline = Pipeline();

    // 【关键修复】始终添加所有步骤，由 ctx.config->stepEnabled 在运行时控制跳过
    // 这样不同ROI可以有不同的步骤组合，不受全局m_config限制
    std::map<int, std::unique_ptr<IPipelineStep>> allSteps;
    allSteps[0] = std::make_unique<StepColorChannel>();
    allSteps[1] = std::make_unique<StepEnhance>(m_processor.get());
    allSteps[2] = std::make_unique<StepFilter>(m_processor.get());
    allSteps[3] = std::make_unique<StepAlgorithmQueue>(m_processor.get(), &m_algorithmQueue);
    allSteps[4] = std::make_unique<StepShapeFilter>();
    allSteps[5] = std::make_unique<StepLineDetector>();
    allSteps[6] = std::make_unique<StepBarcodeRecognition>();
    allSteps[7] = std::make_unique<StepImageFilter>();
    allSteps[8] = std::make_unique<StepOcrRecognition>();

    // 按默认顺序添加所有步骤
    for (int idx : m_config.stepOrder) {
        if (allSteps.contains(idx)) {
            m_pipeline.add(std::move(allSteps[idx]));
        }
    }
}

// ========== per-ROI缓存 ==========

void PipelineManager::cacheResult(const QString& roiId, const PipelineContext& ctx)
{
    if (roiId.isEmpty()) return;
    QMutexLocker locker(&m_roiCacheMutex);
    m_roiCache[roiId] = ctx;
}

PipelineContext PipelineManager::getCachedResult(const QString& roiId) const
{
    QMutexLocker locker(&m_roiCacheMutex);
    return m_roiCache.value(roiId, PipelineContext());
}

bool PipelineManager::hasCachedResult(const QString& roiId) const
{
    QMutexLocker locker(&m_roiCacheMutex);
    return m_roiCache.contains(roiId) && !m_roiCache[roiId].srcBgr.empty();
}

cv::Mat PipelineManager::getCachedDisplay(const QString& roiId, DisplayConfig::Mode mode) const
{
    QMutexLocker locker(&m_roiCacheMutex);
    auto it = m_roiCache.find(roiId);
    if (it == m_roiCache.end() || it->srcBgr.empty()) return cv::Mat();
    return DisplayRenderer::render(*it, mode);
}

void PipelineManager::clearCachedResult(const QString& roiId)
{
    QMutexLocker locker(&m_roiCacheMutex);
    m_roiCache.remove(roiId);
}

void PipelineManager::clearAllCachedResults()
{
    QMutexLocker locker(&m_roiCacheMutex);
    m_roiCache.clear();
}

