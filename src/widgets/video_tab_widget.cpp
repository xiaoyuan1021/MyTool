#include "video_tab_widget.h"
#include "ui_video_tab.h"
#include "logger.h"
#include <QFileDialog>
#include <QMessageBox>

VideoTabWidget::VideoTabWidget(QWidget* parent)
    : QWidget(parent)
    , m_ui(new Ui::VideoTabWidget)
    , m_videoManager(new VideoManager(this))
    , m_isUpdatingProgress(false)
{
    m_ui->setupUi(this);

    // 连接视频管理器的信号
    connect(m_videoManager, &VideoManager::frameReady,
            this, &VideoTabWidget::onFrameReady);
    connect(m_videoManager, &VideoManager::playbackStateChanged,
            this, &VideoTabWidget::onPlaybackStateChanged);
    connect(m_videoManager, &VideoManager::videoOpened,
            this, &VideoTabWidget::onVideoOpened);
    connect(m_videoManager, &VideoManager::videoClosed,
            this, &VideoTabWidget::onVideoClosed);
    connect(m_videoManager, &VideoManager::errorOccurred,
            this, &VideoTabWidget::onErrorOccurred);

    // 初始化相机列表
    updateCameraList();

    // 初始化UI状态
    updateUIState();
}

VideoTabWidget::~VideoTabWidget()
{
    // 先停止视频播放，避免在析构过程中继续处理帧
    if (m_videoManager) {
        m_videoManager->stop();
    }
    delete m_ui;
}

void VideoTabWidget::on_btn_openFile_clicked()
{
    Logger::instance()->info(QString("========== 视频文件选择对话框 =========="));
    
    QString filePath = QFileDialog::getOpenFileName(
        this,
        "选择视频文件",
        "",
        "视频文件 (*.mp4 *.avi *.mov *.mkv *.wmv);;所有文件 (*.*)"
    );

    if (filePath.isEmpty()) {
        Logger::instance()->info("用户取消了文件选择");
        return;
    }

    Logger::instance()->info(QString("用户选择的视频文件: %1").arg(filePath));
    
    // 获取文件信息
    QFileInfo fileInfo(filePath);
    Logger::instance()->info(QString("文件大小: %1 字节").arg(fileInfo.size()));
    
    if (m_videoManager->openFile(filePath)) {
        m_ui->label_status->setText(QString("已加载: %1").arg(fileInfo.fileName()));
        Logger::instance()->info(QString("视频打开成功: %1").arg(fileInfo.fileName()));
    } else {
        Logger::instance()->error(QString("视频打开失败: %1").arg(fileInfo.fileName()));
    }
}

void VideoTabWidget::on_btn_openCamera_clicked()
{
    int currentIndex = m_ui->comboBox_camera->currentIndex();
    if (currentIndex < 0 || currentIndex >= m_cameraList.size()) {
        QMessageBox::warning(this, "警告", "请先选择相机设备");
        return;
    }

    // 从相机列表中提取相机索引
    int cameraIndex = currentIndex;
    if (m_videoManager->openCamera(cameraIndex)) {
        m_ui->label_status->setText(QString("相机 %1 已启动").arg(cameraIndex));
    }
}

void VideoTabWidget::on_btn_play_clicked()
{
    m_videoManager->play();
}

void VideoTabWidget::on_btn_pause_clicked()
{
    m_videoManager->pause();
}

void VideoTabWidget::on_btn_stop_clicked()
{
    m_videoManager->stop();
}

void VideoTabWidget::on_comboBox_camera_currentIndexChanged(int index)
{
    // 相机选择改变时更新UI
    Q_UNUSED(index);
}

void VideoTabWidget::on_slider_progress_valueChanged(int value)
{
    if (m_isUpdatingProgress) {
        return;
    }

    // 进度条控制（仅对本地视频有效）
    if (m_videoManager->getSourceType() == VideoManager::VideoSource::LocalFile) {
        double progress = value / 100.0;
        int targetFrame = static_cast<int>(progress * m_videoManager->getTotalFrames());
        
        // 这里可以实现跳转到指定帧的功能
        // 目前简单实现，后续可以扩展
    }
}

void VideoTabWidget::onFrameReady(const cv::Mat& frame)
{
    if (!frame.empty()) {
        emit videoFrameReady(frame);
        
        // 更新帧率显示
        static int frameCount = 0;
        static QTime lastTime = QTime::currentTime();
        
        frameCount++;
        QTime currentTime = QTime::currentTime();
        if (lastTime.msecsTo(currentTime) >= 1000) {
            m_ui->label_fps->setText(QString("FPS: %1").arg(frameCount));
            frameCount = 0;
            lastTime = currentTime;
        }
        
        // 更新进度条
        updateProgress();
    }
}

void VideoTabWidget::onPlaybackStateChanged(VideoManager::PlaybackState state)
{
    updateUIState();
    
    QString stateText;
    switch (state) {
    case VideoManager::PlaybackState::Playing:
        stateText = "播放中";
        break;
    case VideoManager::PlaybackState::Paused:
        stateText = "已暂停";
        break;
    case VideoManager::PlaybackState::Stopped:
        stateText = "已停止";
        break;
    }
    
    m_ui->label_status->setText(stateText);
}

void VideoTabWidget::onVideoOpened(const QString& source)
{
    m_ui->label_status->setText(QString("已加载: %1").arg(source));
    updateUIState();
    
    // 获取视频信息并显示
    cv::Size frameSize = m_videoManager->getFrameSize();
    double frameRate = m_videoManager->getFrameRate();
    int totalFrames = m_videoManager->getTotalFrames();
    
    QString info = QString("大小: %1x%2 | 帧率: %3 | 帧数: %4")
                  .arg(frameSize.width)
                  .arg(frameSize.height)
                  .arg(frameRate, 0, 'f', 1)
                  .arg(totalFrames);
    
    m_ui->label_videoInfo->setText(info);
}

void VideoTabWidget::onVideoClosed()
{
    m_ui->label_status->setText("无视频");
    m_ui->label_videoInfo->clear();
    m_ui->label_fps->clear();
    updateUIState();
}

void VideoTabWidget::onErrorOccurred(const QString& error)
{
    QMessageBox::critical(this, "错误", error);
    m_ui->label_status->setText("错误");
}

void VideoTabWidget::updateUIState()
{
    bool isOpened = m_videoManager->isOpened();
    bool isPlaying = m_videoManager->getPlaybackState() == VideoManager::PlaybackState::Playing;
    
    m_ui->btn_play->setEnabled(isOpened && !isPlaying);
    m_ui->btn_pause->setEnabled(isOpened && isPlaying);
    m_ui->btn_stop->setEnabled(isOpened);
    
    m_ui->comboBox_camera->setEnabled(!isOpened);
    m_ui->btn_openFile->setEnabled(!isOpened);
    m_ui->btn_openCamera->setEnabled(!isOpened);
    
    // 进度条仅对本地视频有效
    bool isLocalFile = m_videoManager->getSourceType() == VideoManager::VideoSource::LocalFile;
    m_ui->slider_progress->setEnabled(isLocalFile && isOpened);
}

void VideoTabWidget::updateCameraList()
{
    m_cameraList = VideoManager::getAvailableCameras();
    
    m_ui->comboBox_camera->clear();
    for (const QString& camera : m_cameraList) {
        m_ui->comboBox_camera->addItem(camera);
    }
    
    if (m_cameraList.isEmpty()) {
        m_ui->comboBox_camera->addItem("无可用相机");
        m_ui->btn_openCamera->setEnabled(false);
    } else {
        m_ui->btn_openCamera->setEnabled(true);
    }
}

void VideoTabWidget::updateProgress()
{
    if (m_videoManager->getSourceType() != VideoManager::VideoSource::LocalFile) {
        return;
    }
    
    m_isUpdatingProgress = true;
    
    double progress = m_videoManager->getProgress();
    int value = static_cast<int>(progress * 100);
    m_ui->slider_progress->setValue(value);
    
    // 更新时间显示
    int currentFrame = m_videoManager->getCurrentFrameIndex();
    int totalFrames = m_videoManager->getTotalFrames();
    double frameRate = m_videoManager->getFrameRate();
    
    if (frameRate > 0) {
        int currentTime = static_cast<int>(currentFrame / frameRate);
        int totalTime = static_cast<int>(totalFrames / frameRate);
        
        QString timeText = QString("%1:%2 / %3:%4")
                          .arg(currentTime / 60)
                          .arg(currentTime % 60, 2, 10, QChar('0'))
                          .arg(totalTime / 60)
                          .arg(totalTime % 60, 2, 10, QChar('0'));
        
        m_ui->label_time->setText(timeText);
    }
    
    m_isUpdatingProgress = false;
}