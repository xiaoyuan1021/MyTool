#include "image_utils.h"

QImage ImageUtils::matToQImage(const cv::Mat &mat)
{
    if (mat.empty()) return QImage();

    QImage result;

    // 单通道灰度
    if (mat.type() == CV_8UC1) {
        // 优化：只在必要时才拷贝（Mat连续且生命周期可控时可避免拷贝）
        if (mat.isContinuous()) {
            result = QImage(mat.data, mat.cols, mat.rows, mat.step, QImage::Format_Grayscale8).copy();
        } else {
            cv::Mat contiguous = mat.clone();
            result = QImage(contiguous.data, contiguous.cols, contiguous.rows, contiguous.step, QImage::Format_Grayscale8).copy();
        }
    }
    // 3 通道 BGR -> 转为 RGB 给 QImage
    else if (mat.type() == CV_8UC3) {
        // 优化：使用 QImage 直接构造后原地转换，避免额外的 Mat 分配
        QImage image(mat.cols, mat.rows, QImage::Format_RGB888);
        // 逐行拷贝并转换 BGR -> RGB
        for (int row = 0; row < mat.rows; ++row) {
            const uchar* srcRow = mat.ptr<uchar>(row);
            uchar* dstRow = image.scanLine(row);
            for (int col = 0; col < mat.cols; ++col) {
                dstRow[col * 3 + 0] = srcRow[col * 3 + 2]; // R <- B
                dstRow[col * 3 + 1] = srcRow[col * 3 + 1]; // G <- G
                dstRow[col * 3 + 2] = srcRow[col * 3 + 0]; // B <- R
            }
        }
        result = image;
    }
    // 4 通道 BGRA -> QImage ARGB32
    else if (mat.type() == CV_8UC4) {
        // 优化：直接构造 QImage 并逐行拷贝转换
        QImage image(mat.cols, mat.rows, QImage::Format_RGBA8888);
        for (int row = 0; row < mat.rows; ++row) {
            const uchar* srcRow = mat.ptr<uchar>(row);
            uchar* dstRow = image.scanLine(row);
            memcpy(dstRow, srcRow, mat.cols * 4); // BGRA 和 RGBA 只是通道顺序不同，直接拷贝
        }
        result = image;
    }
    else {
        // 其它不支持的类型
        qDebug() << "Unsupported Mat type in matToQImage(): " << mat.type();
        return QImage();
    }

    return result;
}

cv::Mat ImageUtils::qImageToMat(const QImage &image, bool clone)
{
    if (image.isNull())
        return cv::Mat();

    cv::Mat mat;

    switch (image.format())
    {
    // 32位 ARGB
    case QImage::Format_ARGB32:
    case QImage::Format_ARGB32_Premultiplied:
    case QImage::Format_RGB32:
    {
        mat = cv::Mat(image.height(),
                      image.width(),
                      CV_8UC4,
                      const_cast<uchar*>(image.bits()),
                      image.bytesPerLine());
        break;
    }

    // 24位 RGB
    case QImage::Format_RGB888:
    {
        mat = cv::Mat(image.height(),
                      image.width(),
                      CV_8UC3,
                      const_cast<uchar*>(image.bits()),
                      image.bytesPerLine());

        // Qt 是 RGB，OpenCV 是 BGR，需要转换
        cv::cvtColor(mat, mat, cv::COLOR_RGB2BGR);
        break;
    }

    // 8位 灰度
    case QImage::Format_Grayscale8:
    {
        mat = cv::Mat(image.height(),
                      image.width(),
                      CV_8UC1,
                      const_cast<uchar*>(image.bits()),
                      image.bytesPerLine());
        break;
    }

    default:
    {
        qDebug() << "Unsupported QImage format in qImageToMat(): " << image.format();
        return cv::Mat();
    }
    }

    // 是否深拷贝，防止 Qt 内存释放导致 Mat 野指针
    return clone ? mat.clone() : mat;
}


cv::Rect ImageUtils::mapRoiToCvRect(const QRectF& roiRect, int imageCols, int imageRows)
{
    if (imageCols <= 0 || imageRows <= 0)
        return cv::Rect();

    int x = std::max(0, (int)std::floor(roiRect.x()));
    int y = std::max(0, (int)std::floor(roiRect.y()));
    int w = std::min((int)std::ceil(roiRect.width()), imageCols - x);
    int h = std::min((int)std::ceil(roiRect.height()), imageRows - y);

    if (w <= 0 || h <= 0)
        return cv::Rect();

    return cv::Rect(x, y, w, h);
}

cv::Mat ImageUtils::makeStructElement(int ksize)
{
    return cv::getStructuringElement(cv::MORPH_ELLIPSE,
                                 cv::Size(2*ksize+1, 2*ksize+1),
                                 cv::Point(ksize, ksize));
}

QRect ImageUtils::mapLabelToImage(const QRect &rect, const cv::Mat &img, QLabel *label)
{
    if(img.empty() || label==nullptr) return QRect();

    const int labelW=label->width();
    const int labelH=label->height();
    if(labelW<0 || labelH<0) return QRect();

    // 转换 QLabel 坐标到 Mat 坐标
    double scaleX = static_cast<double>(img.cols) / labelW;
    double scaleY = static_cast<double>(img.rows) / labelH;

    int x = static_cast<int>(std::round(rect.x() * scaleX));
    int y = static_cast<int>(std::round(rect.y() * scaleY));
    int w = static_cast<int>(std::round(rect.width() * scaleX));
    int h = static_cast<int>(std::round(rect.height() * scaleY));

    // clamp 矩形，避免越界或负值
    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    if (x + w > img.cols) w = img.cols - x;
    if (y + h > img.rows) h = img.rows - y;
    if (w <= 0 || h <= 0) return QRect();

    return QRect(x,y,w,h);
}



