#include "file_manager.h"
#include "logger.h"
#include <QFileDialog>
#include <QCoreApplication>

FileManager::FileManager(QObject *parent)
    : QObject(parent)
{
}

FileManager::~FileManager()
{
}

QString FileManager::selectImageFile(const QString& defaultPath)
{
    QString path = QFileDialog::getOpenFileName(
        nullptr,  // parent widget
        "请选择图片",
        defaultPath,
        "Image Files (*.jpg *.png *.tif);;All Files (*)"
    );
    return path;
}

void FileManager::readImageFile(const QString& filePath)
{
    if (filePath.isEmpty()) {
        emit errorOccurred("文件路径为空");
        return;
    }

    cv::Mat img = cv::imread(filePath.toStdString());
    if (img.empty()) {
        emit errorOccurred(QString("无法读取图像文件: %1").arg(filePath));
        return;
    }

    emit imageLoaded(img);
}

bool FileManager::saveImageFile(const QString& filePath, const cv::Mat& image)
{
    if (filePath.isEmpty()) {
        emit errorOccurred("文件路径为空");
        return false;
    }

    if (image.empty()) {
        emit errorOccurred("图像数据为空");
        return false;
    }

    bool success = cv::imwrite(filePath.toStdString(), image);
    if (success) {
        emit imageSaved(filePath);
        return true;
    } else {
        emit errorOccurred(QString("保存图像失败: %1").arg(filePath));
        return false;
    }
}
