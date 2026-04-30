#include "widgets/video_source_widget.h"
#include "logger.h"
#include "video_manager.h"
#include <QFileDialog>
#include <QMessageBox>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QComboBox>
#include <QPushButton>
#include <QLineEdit>
#include <QGroupBox>

VideoSourceWidget::VideoSourceWidget(QWidget* parent)
    : QWidget(parent)
    , m_ui(nullptr)
{
    // 直接创建UI，不使用ui文件
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    
    // 视频源选择组
    QGroupBox* group = new QGroupBox("视频源配置", this);
    QVBoxLayout* groupLayout = new QVBoxLayout(group);
    
    // 视频源类型选择
    QHBoxLayout* typeLayout = new QHBoxLayout();
    QLabel* typeLabel = new QLabel("类型:", group);
    QComboBox* comboType = new QComboBox(group);
    comboType->addItem("无", 0);
    comboType->addItem("相机", 1);
    comboType->addItem("视频文件", 2);
    comboType->addItem("图像目录", 3);
    typeLayout->addWidget(typeLabel);
    typeLayout->addWidget(comboType);
    groupLayout->addLayout(typeLayout);
    
    // 视频文件选择
    QHBoxLayout* fileLayout = new QHBoxLayout();
    QLabel* fileLabel = new QLabel("文件:", group);
    QLineEdit* lineEdit = new QLineEdit(group);
    lineEdit->setPlaceholderText("选择视频文件...");
    lineEdit->setReadOnly(true);
    QPushButton* btnOpen = new QPushButton("浏览", group);
    fileLayout->addWidget(fileLabel);
    fileLayout->addWidget(lineEdit);
    fileLayout->addWidget(btnOpen);
    groupLayout->addLayout(fileLayout);
    
    // 图像目录选择
    QHBoxLayout* dirLayout = new QHBoxLayout();
    QLabel* dirLabel = new QLabel("目录:", group);
    QLineEdit* lineDir = new QLineEdit(group);
    lineDir->setPlaceholderText("选择图像目录...");
    lineDir->setReadOnly(true);
    QPushButton* btnDir = new QPushButton("浏览", group);
    dirLayout->addWidget(dirLabel);
    dirLayout->addWidget(lineDir);
    dirLayout->addWidget(btnDir);
    groupLayout->addLayout(dirLayout);
    
    // 相机选择
    QHBoxLayout* camLayout = new QHBoxLayout();
    QLabel* camLabel = new QLabel("相机:", group);
    QComboBox* comboCam = new QComboBox(group);
    QPushButton* btnCam = new QPushButton("打开", group);
    camLayout->addWidget(camLabel);
    camLayout->addWidget(comboCam);
    camLayout->addWidget(btnCam);
    groupLayout->addLayout(camLayout);
    
    mainLayout->addWidget(group);
    
    // 保存UI组件指针（使用简单的结构）
    struct VideoSourceUI {
        QComboBox* comboSourceType;
        QLineEdit* lineEditFilePath;
        QLineEdit* lineEditDirPath;
        QComboBox* comboCamera;
        QPushButton* btnOpenFile;
        QPushButton* btnOpenDir;
        QPushButton* btnOpenCamera;
    };
    
    // 使用静态存储（简化实现）
    static VideoSourceUI uiStorage;
    uiStorage.comboSourceType = comboType;
    uiStorage.lineEditFilePath = lineEdit;
    uiStorage.lineEditDirPath = lineDir;
    uiStorage.comboCamera = comboCam;
    uiStorage.btnOpenFile = btnOpen;
    uiStorage.btnOpenDir = btnDir;
    uiStorage.btnOpenCamera = btnCam;
    
    // 初始化相机列表
    updateCameraList();
    
    // 连接信号
    connect(comboType, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &VideoSourceWidget::on_comboSourceType_currentIndexChanged);
    connect(btnOpen, &QPushButton::clicked,
            this, &VideoSourceWidget::on_btnOpenFile_clicked);
    connect(btnDir, &QPushButton::clicked,
            this, &VideoSourceWidget::on_btnOpenDir_clicked);
    connect(btnCam, &QPushButton::clicked,
            this, &VideoSourceWidget::on_btnOpenCamera_clicked);
    connect(comboCam, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &VideoSourceWidget::on_comboCamera_currentIndexChanged);
    
    updateUIState();
}

VideoSourceWidget::~VideoSourceWidget()
{
}

VisionInspectionConfig::VideoSourceType VideoSourceWidget::getSourceType() const
{
    // 简化实现
    return VisionInspectionConfig::VideoSourceType::None;
}

QString VideoSourceWidget::getVideoFilePath() const
{
    return "";
}

QString VideoSourceWidget::getImageDirectory() const
{
    return "";
}

int VideoSourceWidget::getCameraIndex() const
{
    return 0;
}

void VideoSourceWidget::setSourceType(VisionInspectionConfig::VideoSourceType type)
{
    Q_UNUSED(type);
}

void VideoSourceWidget::setVideoFilePath(const QString& path)
{
    Q_UNUSED(path);
}

void VideoSourceWidget::setImageDirectory(const QString& dir)
{
    Q_UNUSED(dir);
}

void VideoSourceWidget::setCameraIndex(int index)
{
    Q_UNUSED(index);
}

void VideoSourceWidget::loadFromConfig(const VisionInspectionConfig& config)
{
    setSourceType(config.videoSourceType);
    setVideoFilePath(config.videoFilePath);
    setImageDirectory(config.imageDirectory);
    setCameraIndex(config.cameraIndex);
    updateUIState();
}

void VideoSourceWidget::saveToConfig(VisionInspectionConfig& config) const
{
    config.videoSourceType = getSourceType();
    config.videoFilePath = getVideoFilePath();
    config.imageDirectory = getImageDirectory();
    config.cameraIndex = getCameraIndex();
}

QVector<QString> VideoSourceWidget::getAvailableCameras() const
{
    return VideoManager::getAvailableCameras();
}

void VideoSourceWidget::updateCameraList()
{
    m_cameraList = getAvailableCameras();
}

void VideoSourceWidget::on_comboSourceType_currentIndexChanged(int index)
{
    Q_UNUSED(index);
    updateUIState();
    emit sourceTypeChanged(getSourceType());
}

void VideoSourceWidget::on_btnOpenFile_clicked()
{
    QString filePath = QFileDialog::getOpenFileName(
        this,
        "选择视频文件",
        "",
        "视频文件 (*.mp4 *.avi *.mov *.mkv *.wmv);;所有文件 (*.*)"
    );
    
    if (!filePath.isEmpty()) {
        emit videoFileSelected(filePath);
        emit openVideoFileRequested();
    }
}

void VideoSourceWidget::on_btnOpenDir_clicked()
{
    QString dir = QFileDialog::getExistingDirectory(
        this,
        "选择图像目录",
        "",
        QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks
    );
    
    if (!dir.isEmpty()) {
        emit imageDirectorySelected(dir);
        emit openImageDirRequested();
    }
}

void VideoSourceWidget::on_btnOpenCamera_clicked()
{
    int index = 0; // 简化实现
    emit cameraSelected(index);
    emit openCameraRequested();
}

void VideoSourceWidget::on_comboCamera_currentIndexChanged(int index)
{
    Q_UNUSED(index);
}

void VideoSourceWidget::updateUIState()
{
    VisionInspectionConfig::VideoSourceType type = getSourceType();
    
    bool isFile = (type == VisionInspectionConfig::VideoSourceType::VideoFile);
    bool isDir = (type == VisionInspectionConfig::VideoSourceType::ImageDir);
    bool isCamera = (type == VisionInspectionConfig::VideoSourceType::Camera);
    
    // 更新UI可见性（简化实现）
    Q_UNUSED(isFile);
    Q_UNUSED(isDir);
    Q_UNUSED(isCamera);
}