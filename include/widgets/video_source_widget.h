#pragma once

#include <QWidget>
#include <QString>
#include <QVector>
#include "config/detection_type.h"

// 前向声明（避免循环依赖）
struct VideoSourceConfig;

namespace Ui {
class VideoSourceWidget;
}

/**
 * @brief 视频源组件 - 可复用的视频源选择和配置组件
 * 
 * 从VideoTabWidget中提取视频源选择逻辑，用于目标检测等场景。
 */
class VideoSourceWidget : public QWidget
{
    Q_OBJECT

public:
    explicit VideoSourceWidget(QWidget* parent = nullptr);
    ~VideoSourceWidget();
    
    /**
     * @brief 获取当前配置
     */
    VideoSourceType getSourceType() const;
    QString getVideoFilePath() const;
    QString getImageDirectory() const;
    int getCameraIndex() const;
    
    /**
     * @brief 设置配置
     */
    void setSourceType(VideoSourceType type);
    void setVideoFilePath(const QString& path);
    void setImageDirectory(const QString& dir);
    void setCameraIndex(int index);
    
    /**
     * @brief 从配置加载
     */
    void loadFromConfig(const VideoSourceConfig& config);
    
    /**
     * @brief 保存到配置
     */
    void saveToConfig(VideoSourceConfig& config) const;
    
    /**
     * @brief 获取可用相机列表
     */
    QVector<QString> getAvailableCameras() const;
    
    /**
     * @brief 更新相机列表
     */
    void updateCameraList();

signals:
    void sourceTypeChanged(VideoSourceType type);
    void videoFileSelected(const QString& filePath);
    void imageDirectorySelected(const QString& dir);
    void cameraSelected(int index);
    void openVideoFileRequested();
    void openImageDirRequested();
    void openCameraRequested();

private slots:
    void on_comboSourceType_currentIndexChanged(int index);
    void on_btnOpenFile_clicked();
    void on_btnOpenDir_clicked();
    void on_btnOpenCamera_clicked();
    void on_comboCamera_currentIndexChanged(int index);

private:
    void updateUIState();
    
    Ui::VideoSourceWidget* m_ui;
    QVector<QString> m_cameraList;
};