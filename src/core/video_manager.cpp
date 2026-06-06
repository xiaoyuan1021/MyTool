#include "video_manager.h"
#include "logger.h"
#include <QFileInfo>
#include <QDebug>
#include <chrono>

VideoManager::VideoManager(QObject* parent)
    : QObject(parent)
    , m_sourceType(VideoSource::None)
    , m_playbackState(PlaybackState::Stopped)
    , m_frameRate(0.0)
    , m_totalFrames(0)
    , m_currentFrameIndex(0)
    , m_cameraIndex(-1)
{
    m_timer = new QTimer(this);
    connect(m_timer, &QTimer::timeout, this, &VideoManager::onTimeout);
}

VideoManager::~VideoManager()
{
    // 先停止定时器，避免在析构过程中触发超时
    if (m_timer) {
        m_timer->stop();
    }
    
    // 释放视频捕获对象，但不发出信号
    if (m_videoCapture.isOpened()) {
        m_videoCapture.release();
    }
}

bool VideoManager::openFile(const QString& filePath)
{
    Logger::instance()->info(QString("========== 视频打开调试信息 =========="));
    Logger::instance()->info(QString("尝试打开视频文件: %1").arg(filePath));
    
    // 检查文件是否存在
    QFileInfo fileInfo(filePath);
    Logger::instance()->info(QString("文件信息检查:"));
    Logger::instance()->info(QString("  - 文件路径: %1").arg(filePath));
    Logger::instance()->info(QString("  - 文件存在: %1").arg(fileInfo.exists() ? "是" : "否"));
    Logger::instance()->info(QString("  - 是普通文件: %1").arg(fileInfo.isFile() ? "是" : "否"));
    Logger::instance()->info(QString("  - 文件大小: %1 字节").arg(fileInfo.size()));
    Logger::instance()->info(QString("  - 文件权限: %1").arg(fileInfo.isReadable() ? "可读" : "不可读"));
    
    if (!fileInfo.exists() || !fileInfo.isFile()) {
        Logger::instance()->error(QString("视频文件不存在或不是普通文件: %1").arg(filePath));
        emit errorOccurred(QString("视频文件不存在或不是普通文件: %1").arg(filePath));
        Logger::instance()->info(QString("========== 视频打开失败 =========="));
        return false;
    }

    // 先关闭当前视频
    close();

    try {

    // 尝试打开视频文件
    Logger::instance()->info(QString("调用OpenCV打开视频文件..."));
    std::string stdPath = filePath.toLocal8Bit().constData();
    Logger::instance()->info(QString("转换后的标准路径: %1").arg(QString::fromLocal8Bit(stdPath.c_str())));
    
    m_videoCapture.open(stdPath);
    
    Logger::instance()->info(QString("OpenCV打开结果: %1").arg(m_videoCapture.isOpened() ? "成功" : "失败"));
    
    if (!m_videoCapture.isOpened()) {
        Logger::instance()->error(QString("无法打开视频文件: %1").arg(filePath));
        Logger::instance()->info(QString("可能的原因:"));
        Logger::instance()->info(QString("  1. 文件格式不支持"));
        Logger::instance()->info(QString("  2. 缺少视频编解码器"));
        Logger::instance()->info(QString("  3. 文件损坏"));
        Logger::instance()->info(QString("  4. 路径包含中文或特殊字符"));
        emit errorOccurred(QString("无法打开视频文件: %1").arg(filePath));
        Logger::instance()->info(QString("========== 视频打开失败 =========="));
        return false;
    }

    // 获取视频信息
    m_frameRate = m_videoCapture.get(cv::CAP_PROP_FPS);
    m_totalFrames = static_cast<int>(m_videoCapture.get(cv::CAP_PROP_FRAME_COUNT));
    m_currentFrameIndex = 0;
    m_sourceType = VideoSource::LocalFile;
    m_currentSource = filePath;
    m_cameraIndex = -1;

    Logger::instance()->info(QString("打开视频文件: %1 (帧率: %2, 总帧数: %3)")
                            .arg(filePath).arg(m_frameRate).arg(m_totalFrames));
    
    emit videoOpened(filePath);

    cv::Mat frame;
    if (m_videoCapture.read(frame)) {
        m_currentFrame = frame;
        emit frameReady(frame);
    }

    return true;
    } catch (const cv::Exception& ex) {
        QString msg = QString("打开视频文件OpenCV错误: %1").arg(ex.what());
        Logger::instance()->error(msg);
        emit errorOccurred(msg);
        return false;
    } catch (const std::exception& ex) {
        QString msg = QString("打开视频文件异常: %1").arg(ex.what());
        Logger::instance()->error(msg);
        emit errorOccurred(msg);
        return false;
    }
}

bool VideoManager::openCamera(int cameraIndex)
{
    Logger::instance()->info(QString("========== 打开相机操作开始 (设备 %1) ==========").arg(cameraIndex));
    Logger::instance()->info(QString("[调用栈] openCamera(%1) 被调用").arg(cameraIndex));
    
    // 先关闭当前视频
    close();

    try {

    // 打开相机 - 使用DirectShow后端避免MSMF兼容性问题
    Logger::instance()->info(QString("尝试打开相机设备: %1 (使用DirectShow后端)").arg(cameraIndex));
    auto startTime = std::chrono::steady_clock::now();
    m_videoCapture.open(cameraIndex, cv::CAP_DSHOW);
    auto endTime = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count();
    Logger::instance()->info(QString("DirectShow打开结果: %1 (耗时 %2 ms)").arg(m_videoCapture.isOpened() ? "成功" : "失败").arg(duration));
    
    if (!m_videoCapture.isOpened()) {
        // 如果DirectShow失败，尝试MSMF后端
        Logger::instance()->warning(QString("DirectShow打开相机失败，尝试MSMF后端"));
        m_videoCapture.open(cameraIndex);
    }
    
    if (!m_videoCapture.isOpened()) {
        Logger::instance()->error(QString("无法打开相机设备: %1").arg(cameraIndex));
        emit errorOccurred(QString("无法打开相机设备: %1").arg(cameraIndex));
        return false;
    }

    // 不设置分辨率和帧率，使用相机默认值，避免DirectShow二次初始化导致闪烁

    // 获取相机信息
    m_frameRate = m_videoCapture.get(cv::CAP_PROP_FPS);
    m_totalFrames = 0; // 相机流没有固定帧数
    m_currentFrameIndex = 0;
    m_sourceType = VideoSource::Camera;
    m_currentSource = QString("Camera %1").arg(cameraIndex);
    m_cameraIndex = cameraIndex;

    Logger::instance()->info(QString("打开相机设备: %1").arg(cameraIndex));
    
    emit videoOpened(m_currentSource);

    cv::Mat frame;
    if (m_videoCapture.read(frame)) {
        // 相机源左右翻转，提升体验
        cv::flip(frame, frame, 1);
        m_currentFrame = frame;
        emit frameReady(frame);
    }

    return true;
    } catch (const cv::Exception& ex) {
        QString msg = QString("打开相机OpenCV错误: %1").arg(ex.what());
        Logger::instance()->error(msg);
        emit errorOccurred(msg);
        return false;
    } catch (const std::exception& ex) {
        QString msg = QString("打开相机异常: %1").arg(ex.what());
        Logger::instance()->error(msg);
        emit errorOccurred(msg);
        return false;
    }
}

void VideoManager::close()
{
    Logger::instance()->info(QString("[VideoManager] close() 被调用, 当前状态: %1, 是否已打开: %2")
        .arg(static_cast<int>(m_playbackState))
        .arg(m_videoCapture.isOpened() ? "是" : "否"));
    if (m_videoCapture.isOpened()) {
        stop();
        releaseCapture();
        
        m_sourceType = VideoSource::None;
        m_currentSource.clear();
        m_cameraIndex = -1;
        m_currentFrameIndex = 0;
        m_totalFrames = 0;
        m_frameRate = 0.0;
        m_currentFrame.release();

        Logger::instance()->info("视频已关闭");
        emit videoClosed();
    }
}

void VideoManager::play()
{
    if (!m_videoCapture.isOpened()) {
        Logger::instance()->warning("无法播放: 视频未打开");
        return;
    }

    if (m_playbackState == PlaybackState::Playing) {
        return;
    }

    m_playbackState = PlaybackState::Playing;
    
    // 设置定时器间隔（根据帧率）
    int interval = m_frameRate > 0 ? static_cast<int>(1000.0 / m_frameRate) : 33;
    m_timer->start(interval);

    Logger::instance()->info("开始播放");
    emit playbackStateChanged(m_playbackState);
}

void VideoManager::pause()
{
    if (m_playbackState != PlaybackState::Playing) {
        return;
    }

    m_playbackState = PlaybackState::Paused;
    m_timer->stop();

    Logger::instance()->info("暂停播放");
    emit playbackStateChanged(m_playbackState);
}

void VideoManager::stop()
{
    if (m_playbackState == PlaybackState::Stopped) {
        return;
    }

    m_playbackState = PlaybackState::Stopped;
    m_timer->stop();

    // 重置到初始位置
    if (m_videoCapture.isOpened() && m_sourceType == VideoSource::LocalFile) {
        m_videoCapture.set(cv::CAP_PROP_POS_FRAMES, 0);
        m_currentFrameIndex = 0;
        
        // 读取第一帧
        cv::Mat frame;
        if (m_videoCapture.read(frame)) {
            m_currentFrame = frame;
            emit frameReady(frame);
        }
    }

    Logger::instance()->info("停止播放");
    emit playbackStateChanged(m_playbackState);
}

cv::Mat VideoManager::getCurrentFrame()
{
    return m_currentFrame.clone();
}

cv::Mat VideoManager::getNextFrame()
{
    if (!m_videoCapture.isOpened()) {
        return cv::Mat();
    }

    try {
        cv::Mat frame;
        if (m_videoCapture.read(frame)) {
            m_currentFrame = frame;
            m_currentFrameIndex++;
            return frame.clone();
        }

        if (m_sourceType == VideoSource::LocalFile) {
            stop();
        }
    } catch (const cv::Exception& ex) {
        QString msg = QString("读取下一帧OpenCV错误: %1").arg(ex.what());
        Logger::instance()->error(msg);
    } catch (const std::exception& ex) {
        QString msg = QString("读取下一帧异常: %1").arg(ex.what());
        Logger::instance()->error(msg);
    }

    return cv::Mat();
}

double VideoManager::getProgress() const
{
    if (m_totalFrames <= 0 || m_sourceType != VideoSource::LocalFile) {
        return 0.0;
    }
    return static_cast<double>(m_currentFrameIndex) / m_totalFrames;
}

QVector<QString> VideoManager::getAvailableCameras()
{
    QVector<QString> cameras;
    
    Logger::instance()->info("========== 相机枚举开始 ==========");
    auto startTime = std::chrono::steady_clock::now();
    
    // 检测可用的相机设备
    for (int i = 0; i < 10; ++i) {
        Logger::instance()->info(QString("尝试检测相机设备 %1...").arg(i));
        auto deviceStart = std::chrono::steady_clock::now();
        
        cv::VideoCapture cap(i);
        
        auto deviceEnd = std::chrono::steady_clock::now();
        auto deviceDuration = std::chrono::duration_cast<std::chrono::milliseconds>(deviceEnd - deviceStart).count();
        
        if (cap.isOpened()) {
            cameras.append(QString("Camera %1").arg(i));
            Logger::instance()->info(QString("  相机 %1 可用 (耗时 %2 ms)").arg(i).arg(deviceDuration));
            cap.release();
        } else {
            Logger::instance()->info(QString("  相机 %1 不可用 (耗时 %2 ms)").arg(i).arg(deviceDuration));
        }
    }
    
    auto endTime = std::chrono::steady_clock::now();
    auto totalDuration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count();
    Logger::instance()->info(QString("相机枚举完成: 发现 %1 个相机 (总耗时 %2 ms)").arg(cameras.size()).arg(totalDuration));
    Logger::instance()->info("========== 相机枚举结束 ==========");
    
    return cameras;
}

cv::Size VideoManager::getFrameSize() const
{
    if (!m_videoCapture.isOpened()) {
        return cv::Size(0, 0);
    }
    
    int width = static_cast<int>(m_videoCapture.get(cv::CAP_PROP_FRAME_WIDTH));
    int height = static_cast<int>(m_videoCapture.get(cv::CAP_PROP_FRAME_HEIGHT));
    return cv::Size(width, height);
}

void VideoManager::onTimeout()
{
    if (!m_videoCapture.isOpened() || m_playbackState != PlaybackState::Playing) {
        return;
    }

    try {
        cv::Mat frame;
        if (m_videoCapture.read(frame)) {
            // 相机源左右翻转，提升体验
            if (m_sourceType == VideoSource::Camera) {
                cv::flip(frame, frame, 1);
            }
            m_currentFrame = frame;
            m_currentFrameIndex++;
            emit frameReady(frame);
        } else {
            if (m_sourceType == VideoSource::LocalFile) {
                stop();
            }
        }
    } catch (const cv::Exception& ex) {
        QString msg = QString("视频帧读取OpenCV错误: %1").arg(ex.what());
        Logger::instance()->error(msg);
    } catch (const std::exception& ex) {
        QString msg = QString("视频帧读取异常: %1").arg(ex.what());
        Logger::instance()->error(msg);
    }
}

void VideoManager::seekToFrame(int frameIndex)
{
    if (!m_videoCapture.isOpened() || m_sourceType != VideoSource::LocalFile) {
        return;
    }

    if (frameIndex < 0) frameIndex = 0;
    if (m_totalFrames > 0 && frameIndex >= m_totalFrames) {
        frameIndex = m_totalFrames - 1;
    }

    try {
        m_videoCapture.set(cv::CAP_PROP_POS_FRAMES, static_cast<double>(frameIndex));
        m_currentFrameIndex = frameIndex;

        cv::Mat frame;
        if (m_videoCapture.read(frame)) {
            m_currentFrame = frame;
            emit frameReady(frame);
        }
    } catch (const cv::Exception& ex) {
        QString msg = QString("跳转帧OpenCV错误: %1").arg(ex.what());
        Logger::instance()->error(msg);
    } catch (const std::exception& ex) {
        QString msg = QString("跳转帧异常: %1").arg(ex.what());
        Logger::instance()->error(msg);
    }
}

void VideoManager::updateFrameRate()
{
    if (m_videoCapture.isOpened()) {
        m_frameRate = m_videoCapture.get(cv::CAP_PROP_FPS);
    }
}

void VideoManager::releaseCapture()
{
    if (m_videoCapture.isOpened()) {
        m_videoCapture.release();
    }
}