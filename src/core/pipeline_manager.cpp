#include "pipeline_manager.h"
#include "barcode_step.h"
#include <QDebug>
#include <QCryptographicHash>
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

PipelineContext PipelineManager::execute(const cv::Mat& inputImage)
{
    if (inputImage.empty())
    {
        return PipelineContext();
    }

    // ✅ 检查缓存是否有效
    if (isCacheValid(inputImage))
    {
        qDebug() << "[PipelineManager] 使用缓存结果，跳过Pipeline执行";
        // 更新 lastContext 为缓存结果
        m_lastContext = m_cachedContext;
        // 发出完成信号
        emit pipelineFinished("Pipeline执行完成（使用缓存）");
        // 返回缓存的副本
        return m_cachedContext;
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
    m_lastContext.displayConfig.overlayAlpha = m_overlayAlpha;

    // 执行Pipeline
    m_pipeline.run(m_lastContext);

    // ✅ 更新缓存
    m_cachedContext = m_lastContext;
    m_lastConfigHash = calculateConfigHash(inputImage);
    m_lastImageHash = calculateConfigHash(inputImage);

    // 发出完成信号
    QString message = m_lastContext.reason.isEmpty()
                          ? "Pipeline执行完成"
                          : m_lastContext.reason;
    emit pipelineFinished(message);

    // ✅ 返回副本，避免多线程竞争
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

// ========== 缓存相关方法 ==========

QString PipelineManager::calculateConfigHash(const cv::Mat& inputImage) const
{
    QByteArray hashData;

    // 1. 图像数据哈希（采样关键像素，避免全图哈希的开销）
    if (!inputImage.empty())
    {
        // 采样图像的关键点：四个角和中心
        int rows = inputImage.rows;
        int cols = inputImage.cols;
        
        // 添加图像尺寸
        hashData.append(reinterpret_cast<const char*>(&rows), sizeof(int));
        hashData.append(reinterpret_cast<const char*>(&cols), sizeof(int));
        
        // 采样5个关键点的像素值
        if (rows > 0 && cols > 0)
        {
            // 左上角
            const uchar* topLeft = inputImage.ptr<uchar>(0);
            hashData.append(reinterpret_cast<const char*>(topLeft), (std::min)(cols, 10));
            
            // 右上角
            const uchar* topRight = inputImage.ptr<uchar>(0) + (cols - 1) * inputImage.channels();
            hashData.append(reinterpret_cast<const char*>(topRight), (std::min)(cols, 10));

            // 中心
            int centerY = rows / 2;
            int centerX = cols / 2;
            const uchar* center = inputImage.ptr<uchar>(centerY) + centerX * inputImage.channels();
            hashData.append(reinterpret_cast<const char*>(center), (std::min)(cols, 10));
            
            // 左下角
            const uchar* bottomLeft = inputImage.ptr<uchar>(rows - 1);
            hashData.append(reinterpret_cast<const char*>(bottomLeft), (std::min)(cols, 10));

            // 右下角
            const uchar* bottomRight = inputImage.ptr<uchar>(rows - 1) + (cols - 1) * inputImage.channels();
            hashData.append(reinterpret_cast<const char*>(bottomRight), (std::min)(cols, 10));
        }
    }

    // 2. 通道模式
    int channelValue = static_cast<int>(m_config.channel);
    hashData.append(reinterpret_cast<const char*>(&channelValue), sizeof(int));

    // 3. 增强参数
    int brightnessValue = m_config.brightness;
    hashData.append(reinterpret_cast<const char*>(&brightnessValue), sizeof(int));
    double contrastValue = m_config.contrast;
    hashData.append(reinterpret_cast<const char*>(&contrastValue), sizeof(double));
    double gammaValue = m_config.gamma;
    hashData.append(reinterpret_cast<const char*>(&gammaValue), sizeof(double));
    double sharpenValue = m_config.sharpen;
    hashData.append(reinterpret_cast<const char*>(&sharpenValue), sizeof(double));

    // 4. 灰度过滤参数
    int enableGrayFilterValue = static_cast<int>(m_config.enableGrayFilter);
    hashData.append(reinterpret_cast<const char*>(&enableGrayFilterValue), sizeof(int));
    int grayLowValue = m_config.grayLow;
    hashData.append(reinterpret_cast<const char*>(&grayLowValue), sizeof(int));
    int grayHighValue = m_config.grayHigh;
    hashData.append(reinterpret_cast<const char*>(&grayHighValue), sizeof(int));

    // 5. 颜色过滤参数
    int enableColorFilterValue = static_cast<int>(m_config.enableColorFilter);
    hashData.append(reinterpret_cast<const char*>(&enableColorFilterValue), sizeof(int));
    int colorFilterModeValue = static_cast<int>(m_config.colorFilterMode);
    hashData.append(reinterpret_cast<const char*>(&colorFilterModeValue), sizeof(int));
    int rLowValue = m_config.rLow;
    hashData.append(reinterpret_cast<const char*>(&rLowValue), sizeof(int));
    int rHighValue = m_config.rHigh;
    hashData.append(reinterpret_cast<const char*>(&rHighValue), sizeof(int));
    int gLowValue = m_config.gLow;
    hashData.append(reinterpret_cast<const char*>(&gLowValue), sizeof(int));
    int gHighValue = m_config.gHigh;
    hashData.append(reinterpret_cast<const char*>(&gHighValue), sizeof(int));
    int bLowValue = m_config.bLow;
    hashData.append(reinterpret_cast<const char*>(&bLowValue), sizeof(int));
    int bHighValue = m_config.bHigh;
    hashData.append(reinterpret_cast<const char*>(&bHighValue), sizeof(int));
    int hLowValue = m_config.hLow;
    hashData.append(reinterpret_cast<const char*>(&hLowValue), sizeof(int));
    int hHighValue = m_config.hHigh;
    hashData.append(reinterpret_cast<const char*>(&hHighValue), sizeof(int));
    int sLowValue = m_config.sLow;
    hashData.append(reinterpret_cast<const char*>(&sLowValue), sizeof(int));
    int sHighValue = m_config.sHigh;
    hashData.append(reinterpret_cast<const char*>(&sHighValue), sizeof(int));
    int vLowValue = m_config.vLow;
    hashData.append(reinterpret_cast<const char*>(&vLowValue), sizeof(int));
    int vHighValue = m_config.vHigh;
    hashData.append(reinterpret_cast<const char*>(&vHighValue), sizeof(int));

    // 6. 算法队列
    int algorithmQueueSize = static_cast<int>(m_algorithmQueue.size());
    hashData.append(reinterpret_cast<const char*>(&algorithmQueueSize), sizeof(int));
    for (const auto& step : m_algorithmQueue)
    {
        // step.type 是 QString 类型，需要转换为字节数组
        hashData.append(step.type.toUtf8());
        // 添加参数
        int paramsSize = static_cast<int>(step.params.size());
        hashData.append(reinterpret_cast<const char*>(&paramsSize), sizeof(int));
        for (const auto& param : step.params)
        {
            hashData.append(reinterpret_cast<const char*>(&param), sizeof(double));
        }
    }

    // 7. 显示模式
    int displayModeValue = static_cast<int>(m_displayMode);
    hashData.append(reinterpret_cast<const char*>(&displayModeValue), sizeof(int));

    // 8. 计算哈希
    QCryptographicHash hash(QCryptographicHash::Md5);
    hash.addData(hashData);
    return hash.result().toHex();
}

bool PipelineManager::isCacheValid(const cv::Mat& inputImage) const
{
    // 如果缓存为空，无效
    if (m_lastConfigHash.isEmpty())
    {
        return false;
    }

    // 计算当前配置哈希
    QString currentHash = calculateConfigHash(inputImage);

    // 比较哈希值
    return currentHash == m_lastConfigHash;
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

    // 6️⃣ 清空缓存
    m_lastConfigHash.clear();
    m_lastImageHash.clear();
    m_cachedContext = PipelineContext();
    //qDebug() << "[PipelineManager]   - 缓存已清空";

    // 7️⃣ 重新初始化Pipeline
    initPipeline();
    //qDebug() << "[PipelineManager]   - Pipeline已重新初始化";

    // 8️⃣ 发出信号
    emit pipelineReset();
    emit algorithmQueueChanged(0);

    //qDebug() << "[PipelineManager] ===== Pipeline重置完成 =====";
}
