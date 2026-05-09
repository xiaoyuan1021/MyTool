#ifndef PATH_UTILS_H
#define PATH_UTILS_H

#include <QString>
#include <QFile>
#include <QByteArray>
#include <vector>
#include <string>
#include "opencv2/imgcodecs.hpp"

/**
 * @namespace PathUtils
 * @brief 提供跨平台的文件路径工具函数
 *
 * 使用 Qt 的 QFile 来读写文件，避免 OpenCV 内部 fopen() 在 Windows 上
 * 无法处理中文路径（UTF-8 vs GBK 编码问题）。
 */
namespace PathUtils {

/**
 * @brief 读取图像文件（支持中文路径）
 * @param filePath 图像文件路径
 * @param flags OpenCV 读取标志（默认 IMREAD_UNCHANGED）
 * @return 读取的 cv::Mat，失败返回空 Mat
 */
inline cv::Mat readImageFromFile(const QString& filePath, int flags = cv::IMREAD_UNCHANGED)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        return cv::Mat();
    }
    QByteArray data = file.readAll();
    file.close();

    if (data.isEmpty()) {
        return cv::Mat();
    }

    std::vector<uchar> buf(data.constBegin(), data.constEnd());
    return cv::imdecode(buf, flags);
}

/**
 * @brief 保存图像到文件（支持中文路径）
 * @param filePath 保存路径（后缀决定格式）
 * @param image 要保存的图像
 * @param params 编码参数（可选）
 * @return 成功返回 true，失败返回 false
 */
inline bool writeImageToFile(const QString& filePath, const cv::Mat& image,
                              const std::vector<int>& params = {})
{
    if (image.empty()) {
        return false;
    }

    // 从文件扩展名获取格式后缀
    std::string pathStr = filePath.toStdString();
    std::string ext;
    size_t dotPos = pathStr.find_last_of('.');
    if (dotPos != std::string::npos) {
        ext = pathStr.substr(dotPos);
    }

    std::vector<uchar> buf;
    if (!cv::imencode(ext.c_str(), image, buf, params)) {
        return false;
    }

    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly)) {
        return false;
    }
    file.write(reinterpret_cast<const char*>(buf.data()), buf.size());
    file.close();

    return true;
}

} // namespace PathUtils

#endif // PATH_UTILS_H