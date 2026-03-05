#ifndef FILE_MANAGER_H
#define FILE_MANAGER_H

#include <QObject>
#include <QString>
#include <opencv2/opencv.hpp>

/**
 * @class FileManager
 * @brief 文件管理器 - 负责图像文件的读写操作
 *
 * 职责：
 * - 打开文件对话框
 * - 读取图像文件
 * - 保存图像文件
 * - 通过信号报告结果和错误
 *
 * 不负责：UI显示、状态管理、Pipeline等业务逻辑
 */
class FileManager : public QObject
{
    Q_OBJECT

public:
    explicit FileManager(QObject *parent = nullptr);
    ~FileManager();

    /**
     * @brief 打开文件对话框，让用户选择图像文件
     * @param defaultPath 默认打开路径
     * @return 用户选择的文件路径，如果取消返回空字符串
     */
    QString selectImageFile(const QString& defaultPath);

    /**
     * @brief 读取图像文件
     * @param filePath 图像文件路径
     *
     * 成功时发出 imageLoaded 信号，失败时发出 errorOccurred 信号
     */
    void readImageFile(const QString& filePath);

    /**
     * @brief 保存图像文件
     * @param filePath 保存路径
     * @param image 要保存的图像
     * @return 是否保存成功，同时发出相应的信号
     */
    bool saveImageFile(const QString& filePath, const cv::Mat& image);

signals:
    /**
     * @signal imageLoaded
     * @brief 图像加载成功时发出
     * @param image 加载的图像数据
     */
    void imageLoaded(const cv::Mat& image);

    /**
     * @signal imageSaved
     * @brief 图像保存成功时发出
     * @param path 保存的文件路径
     */
    void imageSaved(const QString& path);

    /**
     * @signal errorOccurred
     * @brief 发生错误时发出
     * @param message 错误信息
     */
    void errorOccurred(const QString& message);

private:
};

#endif // FILE_MANAGER_H
