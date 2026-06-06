#include "file_manager.h"
#include "pipeline_manager.h"
#include "logger.h"
#include "utils/path_utils.h"
#include <QFileDialog>
#include <QMessageBox>
#include <QCoreApplication>
#include <QDir>

FileManager::FileManager(QObject *parent)
    : QObject(parent)
{
}

FileManager::~FileManager()
{
}

QString FileManager::selectImageFile(const QString& defaultPath)
{
    // [NOTE] 默认打开images文件夹
    QString startPath = defaultPath;
    if (startPath.isEmpty()) {
        // 优先使用项目根目录下的images文件夹
        QString imagesDir = QString(PROJECT_ROOT_DIR) + "/images";
        if (QDir(imagesDir).exists()) {
            startPath = imagesDir;
        } else {
            // 回退到applicationDirPath
            imagesDir = QCoreApplication::applicationDirPath() + "/images";
            if (QDir(imagesDir).exists()) {
                startPath = imagesDir;
            } else {
                startPath = QDir::currentPath();
            }
        }
    }

    QString path = QFileDialog::getOpenFileName(
        nullptr,  // parent widget
        "请选择图片",
        startPath,
        "Image Files (*.jpg *.png *.tif *.bmp);;All Files (*)"
    );
    return path;
}

void FileManager::readImageFile(const QString& filePath)
{
    if (filePath.isEmpty()) {
        emit errorOccurred("文件路径为空");
        return;
    }

    try {
        cv::Mat img = PathUtils::readImageFromFile(filePath);
        if (img.empty()) {
            emit errorOccurred(QString("无法读取图像文件: %1").arg(filePath));
            return;
        }
        emit imageLoaded(img, filePath);
    } catch (const cv::Exception& ex) {
        QString msg = QString("读取图像OpenCV错误: %1").arg(ex.what());
        spdlog::error(msg);
        emit errorOccurred(msg);
    } catch (const std::exception& ex) {
        QString msg = QString("读取图像异常: %1").arg(ex.what());
        spdlog::error(msg);
        emit errorOccurred(msg);
    }
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

    try {
        bool success = PathUtils::writeImageToFile(filePath, image);
        if (success) {
            emit imageSaved(filePath);
            return true;
        } else {
            emit errorOccurred(QString("保存图像失败: %1").arg(filePath));
            return false;
        }
    } catch (const cv::Exception& ex) {
        QString msg = QString("保存图像OpenCV错误: %1").arg(ex.what());
        spdlog::error(msg);
        emit errorOccurred(msg);
        return false;
    } catch (const std::exception& ex) {
        QString msg = QString("保存图像异常: %1").arg(ex.what());
        spdlog::error(msg);
        emit errorOccurred(msg);
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
        nullptr, "保存图片", ".", "*.jpg *.png *.tif *.bmp");

    if (saveName.isEmpty()) return;

    cv::Mat toSave;

    // 如果有mask，询问用户保存什么
    if (!ctx.filterMask.empty() && cv::countNonZero(ctx.filterMask) > 0) {
        QMessageBox::StandardButton reply = QMessageBox::question(
            nullptr, "保存选项",
            "保存原图还是处理后的mask?\n"
            "Yes = 保存mask (黑白图)\n"
            "No = 保存增强后的图像",
            QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel);

        if (reply == QMessageBox::Yes) {
            toSave = ctx.filterMask;
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


