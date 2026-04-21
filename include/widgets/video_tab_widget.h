#ifndef VIDEO_TAB_WIDGET_H
#define VIDEO_TAB_WIDGET_H

#include <QWidget>
#include <QString>
#include <QVector>
#include "video_manager.h"

namespace Ui {
class VideoTabWidget;
}

class VideoTabWidget : public QWidget
{
    Q_OBJECT

public:
    explicit VideoTabWidget(QWidget* parent = nullptr);
    ~VideoTabWidget();

    // 获取视频管理器实例
    VideoManager* getVideoManager() const { return m_videoManager; }

signals:
    void videoFrameReady(const cv::Mat& frame);
    void videoSourceChanged(const QString& source);

private slots:
    void on_btn_openFile_clicked();
    void on_btn_openCamera_clicked();
    void on_btn_play_clicked();
    void on_btn_pause_clicked();
    void on_btn_stop_clicked();
    void on_comboBox_camera_currentIndexChanged(int index);
    void on_slider_progress_valueChanged(int value);

    // 视频管理器信号槽
    void onFrameReady(const cv::Mat& frame);
    void onPlaybackStateChanged(VideoManager::PlaybackState state);
    void onVideoOpened(const QString& source);
    void onVideoClosed();
    void onErrorOccurred(const QString& error);

private:
    void updateUIState();
    void updateCameraList();
    void updateProgress();

    Ui::VideoTabWidget* m_ui;
    VideoManager* m_videoManager;
    QVector<QString> m_cameraList;
    bool m_isUpdatingProgress;
};

#endif // VIDEO_TAB_WIDGET_H