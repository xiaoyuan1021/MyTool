#include "pipeline_scheduler.h"
#include "pipeline_manager.h"
#include "logger.h"
#include <QElapsedTimer>
#include <QtConcurrent/QtConcurrent>

// PipelineRequest 静态成员
QAtomicInt PipelineRequest::s_nextId{0};

PipelineScheduler::PipelineScheduler(PipelineManager* pipeline, QObject* parent)
    : QObject(parent)
    , m_pipeline(pipeline)
    , m_debounceTimer(new QTimer(this))
{
    // 配置消抖定时器
    m_debounceTimer->setSingleShot(true);
    m_debounceTimer->setInterval(m_debounceMs);
    connect(m_debounceTimer, &QTimer::timeout, this, &PipelineScheduler::onDebounceTimeout);

    // 创建 FutureWatcher
    m_watcher = new QFutureWatcher<PipelineResult>(this);
    connect(m_watcher, &QFutureWatcher<PipelineResult>::finished, this, &PipelineScheduler::onTaskFinished);
}

PipelineScheduler::~PipelineScheduler()
{
    cancelAll();
    if (m_watcher && m_watcher->isRunning()) {
        m_watcher->waitForFinished();
    }
}

// ========== 请求接口 ==========

qint64 PipelineScheduler::submit(const cv::Mat& image, const PipelineConfig& config, int priority)
{
    PipelineRequest request(image, config, priority);
    return submit(std::move(request));
}

qint64 PipelineScheduler::submit(PipelineRequest request)
{
    qint64 requestId = request.id();

    QMutexLocker locker(&m_queueMutex);

    // 去重检查：如果队列中已有相同请求，忽略
    if (m_deduplicationEnabled) {
        for (const auto& existing : m_queue) {
            if (existing.id() == requestId) {
                qDebug() << "[PipelineScheduler] 忽略重复请求:" << requestId;
                return requestId;
            }
        }
    }

    // 队列满时，移除最低优先级的请求
    while (m_queue.size() >= m_maxQueueSize) {
        int lowestIdx = 0;
        for (int i = 1; i < m_queue.size(); ++i) {
            if (m_queue[i].priority() < m_queue[lowestIdx].priority()) {
                lowestIdx = i;
            }
        }
        qDebug() << "[PipelineScheduler] 队列满，移除低优先级请求:" << m_queue[lowestIdx].id();
        m_queue.removeAt(lowestIdx);
    }

    // 添加到队列
    m_queue.enqueue(std::move(request));
    qDebug() << "[PipelineScheduler] 请求入队:" << requestId << "队列长度:" << m_queue.size();

    locker.unlock();
    emitQueueChanged();

    // 重启消抖定时器
    m_debounceTimer->start();

    return requestId;
}

void PipelineScheduler::cancel(qint64 requestId)
{
    QMutexLocker locker(&m_queueMutex);

    for (int i = 0; i < m_queue.size(); ++i) {
        if (m_queue[i].id() == requestId) {
            qDebug() << "[PipelineScheduler] 取消请求:" << requestId;
            m_queue.removeAt(i);
            locker.unlock();
            emit cancelled(requestId);
            emitQueueChanged();
            return;
        }
    }
}

void PipelineScheduler::cancelAll()
{
    QMutexLocker locker(&m_queueMutex);

    if (m_queue.isEmpty()) return;

    qDebug() << "[PipelineScheduler] 取消所有请求，队列长度:" << m_queue.size();
    m_queue.clear();
    locker.unlock();

    m_debounceTimer->stop();
    emitQueueChanged();
}

// ========== 配置 ==========

void PipelineScheduler::setDebounceMs(int ms)
{
    m_debounceMs = ms;
    m_debounceTimer->setInterval(ms);
}

void PipelineScheduler::setMaxQueueSize(int size)
{
    m_maxQueueSize = size;
}

void PipelineScheduler::setDeduplicationEnabled(bool enabled)
{
    m_deduplicationEnabled = enabled;
}

// ========== 状态查询 ==========

int PipelineScheduler::pendingCount() const
{
    QMutexLocker locker(&m_queueMutex);
    return m_queue.size();
}

// ========== 内部实现 ==========

void PipelineScheduler::onDebounceTimeout()
{
    processNext();
}

void PipelineScheduler::processNext()
{
    // 如果正在执行，等待完成
    if (m_processing.loadAcquire()) {
        qDebug() << "[PipelineScheduler] 正在执行，等待完成";
        return;
    }

    QMutexLocker locker(&m_queueMutex);

    // 队列为空
    if (m_queue.isEmpty()) {
        return;
    }

    // 取出最高优先级的请求
    int highestIdx = 0;
    for (int i = 1; i < m_queue.size(); ++i) {
        if (m_queue[i].priority() > m_queue[highestIdx].priority()) {
            highestIdx = i;
        }
    }

    PipelineRequest request = std::move(m_queue[highestIdx]);
    m_queue.removeAt(highestIdx);

    locker.unlock();
    emitQueueChanged();

    // 执行请求
    executeRequest(request);
}

void PipelineScheduler::executeRequest(const PipelineRequest& request)
{
    m_processing.storeRelease(1);
    emit processingChanged(true);

    qDebug() << "[PipelineScheduler] 开始执行请求:" << request.id();

    // 捕获需要的指针和数据（按值），避免在后台线程中捕获 this
    PipelineManager* pipeline = m_pipeline;
    PipelineRequest req = request.snapshot();  // 深拷贝，确保线程安全

    QFuture<PipelineResult> future = QtConcurrent::run(
        [pipeline, req = std::move(req)]() mutable -> PipelineResult {
            QElapsedTimer timer;
            timer.start();

            try {
                // 检查 PipelineManager 是否有效
                if (!pipeline) {
                    return PipelineResult::failure(req, "PipelineManager 已释放");
                }

                // 执行 Pipeline
                PipelineContext ctx = pipeline->execute(req.image(), req.config());
                double elapsed = timer.elapsed();

                return PipelineResult::success(std::move(req), std::move(ctx), elapsed);
            } catch (const cv::Exception& ex) {
                double elapsed = timer.elapsed();
                QString error = QString("OpenCV 错误: %1").arg(ex.what());
                Logger::instance()->error(error);
                return PipelineResult::failure(req, error, elapsed);
            } catch (const std::exception& ex) {
                double elapsed = timer.elapsed();
                QString error = QString("异常: %1").arg(ex.what());
                Logger::instance()->error(error);
                return PipelineResult::failure(req, error, elapsed);
            } catch (...) {
                double elapsed = timer.elapsed();
                QString error = "未知异常";
                Logger::instance()->error(error);
                return PipelineResult::failure(req, error, elapsed);
            }
        }
    );

    m_watcher->setFuture(future);
}

void PipelineScheduler::onTaskFinished()
{
    if (!m_watcher) return;

    PipelineResult result = m_watcher->result();

    m_lastExecutedId.storeRelease(result.requestId());
    m_processing.storeRelease(0);
    emit processingChanged(false);

    qDebug() << "[PipelineScheduler] 请求执行完成:" << result.requestId()
             << "耗时:" << result.elapsedMs() << "ms"
             << "成功:" << result.isSuccess();

    emit finished(result);

    // 检查队列中是否还有待处理的请求
    if (pendingCount() > 0) {
        // 延迟处理下一个请求，避免过于频繁
        QTimer::singleShot(10, this, &PipelineScheduler::processNext);
    }
}

void PipelineScheduler::emitQueueChanged()
{
    emit queueChanged(pendingCount());
}
