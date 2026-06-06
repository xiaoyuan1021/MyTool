#include "image_utils.h"
#include "logger.h"

QImage ImageUtils::matToQImage(const cv::Mat &mat)
{
    if (mat.empty()) return QImage();

    try {
        QImage result;

        if (mat.type() == CV_8UC1) {
            if (mat.isContinuous()) {
                result = QImage(mat.data, mat.cols, mat.rows, mat.step, QImage::Format_Grayscale8).copy();
            } else {
                cv::Mat contiguous = mat.clone();
                result = QImage(contiguous.data, contiguous.cols, contiguous.rows, contiguous.step, QImage::Format_Grayscale8).copy();
            }
        }
        else if (mat.type() == CV_8UC3) {
            QImage image(mat.cols, mat.rows, QImage::Format_RGB888);
            for (int row = 0; row < mat.rows; ++row) {
                const uchar* srcRow = mat.ptr<uchar>(row);
                uchar* dstRow = image.scanLine(row);
                for (int col = 0; col < mat.cols; ++col) {
                    dstRow[col * 3 + 0] = srcRow[col * 3 + 2];
                    dstRow[col * 3 + 1] = srcRow[col * 3 + 1];
                    dstRow[col * 3 + 2] = srcRow[col * 3 + 0];
                }
            }
            result = image;
        }
        else if (mat.type() == CV_8UC4) {
            QImage image(mat.cols, mat.rows, QImage::Format_RGBA8888);
            for (int row = 0; row < mat.rows; ++row) {
                const uchar* srcRow = mat.ptr<uchar>(row);
                uchar* dstRow = image.scanLine(row);
                memcpy(dstRow, srcRow, mat.cols * 4);
            }
            result = image;
        }
        else {
            spdlog::info(QString("Unsupported Mat type in matToQImage(): %1").arg(mat.type()));
            return QImage();
        }

        return result;
    } catch (const cv::Exception& ex) {
spdlog::error(QString("Mat转QImage错误: %1").arg(ex.what()));
        return QImage();
    } catch (...) {
        spdlog::info("[matToQImage] 未知异常");
        spdlog::error("Mat转QImage未知异常");
        return QImage();
    }
}

cv::Mat ImageUtils::qImageToMat(const QImage &image, bool clone)
{
    if (image.isNull())
        return cv::Mat();

    try {
        cv::Mat mat;

        switch (image.format())
        {
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

        case QImage::Format_RGB888:
        {
            mat = cv::Mat(image.height(),
                          image.width(),
                          CV_8UC3,
                          const_cast<uchar*>(image.bits()),
                          image.bytesPerLine());
            cv::cvtColor(mat, mat, cv::COLOR_RGB2BGR);
            break;
        }

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
            spdlog::info(QString("Unsupported QImage format in qImageToMat(): %1").arg(static_cast<int>(image.format())));
            return cv::Mat();
        }
        }

        return clone ? mat.clone() : mat;
    } catch (const cv::Exception& ex) {
spdlog::error(QString("QImage转Mat错误: %1").arg(ex.what()));
        return cv::Mat();
    } catch (...) {
        spdlog::info("[qImageToMat] 未知异常");
        spdlog::error("QImage转Mat未知异常");
        return cv::Mat();
    }
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





