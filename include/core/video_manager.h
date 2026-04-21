#ifndef VIDEO_MANAGER_H
#define VIDEO_MANAGER_H

#include <QObject>
#include <QString>
#include <QTimer>
#include <QVector>
#include <opencv2/opencv.hpp>

class VideoManager : public QObject
{
    Q_OBJECT

public:
    enum class VideoSource {
        None,
        LocalFile,
        Camera
    };

    enum class PlaybackState {
        Stopped,
        Playing,
        Paused
    };

    explicit VideoManager(QObject* parent = nullptr);
    ~VideoManager();

    // 视频源控制
    bool openFile(const QString& filePath);
    bool openCamera(int cameraIndex = 0);
    void close();

    // 播放控制
    void play();
    void pause();
    void stop();

    // 帧获取
    cv::Mat getCurrentFrame();
    cv::Mat getNextFrame();

    // 状态查询
    VideoSource getSourceType() const { return m_sourceType; }
    PlaybackState getPlaybackState() const { return m_playbackState; }
    bool isOpened() const { return m_videoCapture.isOpened(); }
    double getFrameRate() const { return m_frameRate; }
    int getTotalFrames() const { return m_totalFrames; }
    int getCurrentFrameIndex() const { return m_currentFrameIndex; }
    double getProgress() const;

    // 相机设备管理
    static QVector<QString> getAvailableCameras();
    int getCurrentCameraIndex() const { return m_cameraIndex; }

    // 视频信息
    QString getCurrentSource() const { return m_currentSource; }
    cv::Size getFrameSize() const;

signals:
    void frameReady(const cv::Mat& frame);
    void playbackStateChanged(PlaybackState state);
    void videoOpened(const QString& source);
    void videoClosed();
    void errorOccurred(const QString& error);

private slots:
    void onTimeout();

private:
    void updateFrameRate();
    void releaseCapture();

    cv::VideoCapture m_videoCapture;
    VideoSource m_sourceType;
    PlaybackState m_playbackState;
    
    QTimer* m_timer;
    double m_frameRate;
    int m_totalFrames;
    int m_currentFrameIndex;
    int m_cameraIndex;
    QString m_currentSource;
    
    cv::Mat m_currentFrame;
};

#endif // VIDEO_MANAGER_H