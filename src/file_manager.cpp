#include "file_manager.h"
#include "pipeline_manager.h"
#include "logger.h"
#include <QFileDialog>
#include <QMessageBox>
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

void FileManager::saveImageFileWithDialog(PipelineManager* pipelineManager)
{
    if (!pipelineManager) {
        emit errorOccurred("Pipeline管理器为空");
        return;
    }

    const PipelineContext& ctx = pipelineManager->getLastContext();

    if (ctx.srcBgr.empty()) {
        QMessageBox::warning(nullptr, "保存失败", "请先打开图片");
        return;
    }

    QString saveName = QFileDialog::getSaveFileName(
        nullptr, "保存图片", ".", "*.jpg *.png *.tif");

    if (saveName.isEmpty()) return;

    cv::Mat toSave;

    // 如果有mask，询问用户保存什么
    if (!ctx.mask.empty() && cv::countNonZero(ctx.mask) > 0) {
        QMessageBox::StandardButton reply = QMessageBox::question(
            nullptr, "保存选项",
            "保存原图还是处理后的mask?\n"
            "Yes = 保存mask (黑白图)\n"
            "No = 保存增强后的图像",
            QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel);

        if (reply == QMessageBox::Yes) {
            toSave = ctx.mask;
        } else if (reply == QMessageBox::No) {
            toSave = ctx.enhanced.empty() ? ctx.srcBgr : ctx.enhanced;
        } else {
            return;
        }
    } else {
        toSave = ctx.enhanced.empty() ? ctx.srcBgr : ctx.enhanced;
    }

    saveImageFile(saveName, toSave);
}
