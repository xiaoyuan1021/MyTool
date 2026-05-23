#include "pipeline_manager.h"
#include "image_processor.h"
#include "barcode_step.h"
#include "ocr_step.h"
#include "config/constants.h"
#include "logger.h"
#include "algorithm/display_renderer.h"
#include "utils/benchmark.h"
#include <QDebug>
#include <algorithm>
#include <map>

PipelineManager::PipelineManager(QObject* parent)
    : QObject(parent)
    , m_processor(std::make_unique<ImageProcessor>())
{
    resetConfigToDefaults();
    initPipeline();
}

PipelineManager::~PipelineManager() = default;

// ========== 配置管理 ==========

void PipelineManager::resetConfigToDefaults()
{
    m_config.resetToDefaults();
    qDebug() << "[PipelineManager] resetConfigToDefaults";
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
        m_algorithmQueue.swapItemsAt(index1, index2);
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
    ctx.visualBase = inputImage;  // 初始化可视化基底为原图
    ctx.config = &config;

    try {
        {
            BenchmarkTimer t("Pipeline::run", &m_lastExecMs);
            m_pipeline.run(ctx);
        }
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

// ========== 非trivial方法 ==========

void PipelineManager::updateAlgorithmStep(int index, const AlgorithmStep& step)
{
    if (index >= 0 && index < m_algorithmQueue.size())
    {
        m_algorithmQueue[index] = step;
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
    m_config.shapeFilter.clear();
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

    std::map<int, std::unique_ptr<IPipelineStep>> allSteps;
    allSteps[0] = std::make_unique<StepColorChannel>();
    allSteps[1] = std::make_unique<StepEnhance>(m_processor.get());
    allSteps[2] = std::make_unique<StepGrayFilter>();
    allSteps[3] = std::make_unique<StepColorFilter>(m_processor.get());
    allSteps[4] = std::make_unique<StepAlgorithmQueue>(m_processor.get(), &m_algorithmQueue);
    allSteps[5] = std::make_unique<StepShapeFilter>();
    allSteps[6] = std::make_unique<StepLineDetect>();
    allSteps[7] = std::make_unique<StepReferenceLineFilter>();
    allSteps[8] = std::make_unique<StepBarcodeRecognition>();
    allSteps[9] = std::make_unique<StepImageFilter>();
    allSteps[10] = std::make_unique<StepOcrRecognition>();

    for (int idx : m_config.stepOrder) {
        if (m_config.stepEnabled[idx] && allSteps.contains(idx)) {
            m_pipeline.add(std::move(allSteps[idx]));
        }
    }
}