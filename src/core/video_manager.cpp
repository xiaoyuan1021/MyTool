#include "video_manager.h"
#include "logger.h"
#include <QFileInfo>

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
    close();
}

bool VideoManager::openFile(const QString& filePath)
{
    // 检查文件是否存在
    QFileInfo fileInfo(filePath);
    if (!fileInfo.exists() || !fileInfo.isFile()) {
        Logger::instance()->error(QString("视频文件不存在: %1").arg(filePath));
        emit errorOccurred(QString("视频文件不存在: %1").arg(filePath));
        return false;
    }

    // 先关闭当前视频
    close();

    // 打开视频文件
    m_videoCapture.open(filePath.toStdString());
    if (!m_videoCapture.isOpened()) {
        Logger::instance()->error(QString("无法打开视频文件: %1").arg(filePath));
        emit errorOccurred(QString("无法打开视频文件: %1").arg(filePath));
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
    
    // 读取第一帧
    cv::Mat frame;
    if (m_videoCapture.read(frame)) {
        m_currentFrame = frame;
        emit frameReady(frame);
    }

    return true;
}

bool VideoManager::openCamera(int cameraIndex)
{
    // 先关闭当前视频
    close();

    // 打开相机
    m_videoCapture.open(cameraIndex);
    if (!m_videoCapture.isOpened()) {
        Logger::instance()->error(QString("无法打开相机设备: %1").arg(cameraIndex));
        emit errorOccurred(QString("无法打开相机设备: %1").arg(cameraIndex));
        return false;
    }

    // 设置相机参数
    m_videoCapture.set(cv::CAP_PROP_FRAME_WIDTH, 1280);
    m_videoCapture.set(cv::CAP_PROP_FRAME_HEIGHT, 720);
    m_videoCapture.set(cv::CAP_PROP_FPS, 30);

    // 获取相机信息
    m_frameRate = m_videoCapture.get(cv::CAP_PROP_FPS);
    m_totalFrames = 0; // 相机流没有固定帧数
    m_currentFrameIndex = 0;
    m_sourceType = VideoSource::Camera;
    m_currentSource = QString("Camera %1").arg(cameraIndex);
    m_cameraIndex = cameraIndex;

    Logger::instance()->info(QString("打开相机设备: %1").arg(cameraIndex));
    
    emit videoOpened(m_currentSource);
    
    // 读取第一帧
    cv::Mat frame;
    if (m_videoCapture.read(frame)) {
        m_currentFrame = frame;
        emit frameReady(frame);
    }

    return true;
}

void VideoManager::close()
{
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

    cv::Mat frame;
    if (m_videoCapture.read(frame)) {
        m_currentFrame = frame;
        m_currentFrameIndex++;
        return frame.clone();
    }

    // 视频结束
    if (m_sourceType == VideoSource::LocalFile) {
        stop();
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
    
    // 检测可用的相机设备
    for (int i = 0; i < 10; ++i) {
        cv::VideoCapture cap(i);
        if (cap.isOpened()) {
            cameras.append(QString("Camera %1").arg(i));
            cap.release();
        }
    }
    
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

    cv::Mat frame;
    if (m_videoCapture.read(frame)) {
        m_currentFrame = frame;
        m_currentFrameIndex++;
        emit frameReady(frame);
    } else {
        // 视频结束
        if (m_sourceType == VideoSource::LocalFile) {
            stop();
        }
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