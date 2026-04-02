#include "image_utils.h"

QImage ImageUtils::matToQImage(const Mat &mat)
{
    if (mat.empty()) return QImage();

    // 单通道灰度
    if (mat.type() == CV_8UC1) {
        return QImage(mat.data, mat.cols, mat.rows, mat.step, QImage::Format_Grayscale8).copy();
    }

    // 3 通道 BGR -> 转为 RGB 给 QImage
    if (mat.type() == CV_8UC3) {
        Mat rgb;
        cvtColor(mat, rgb, cv::COLOR_BGR2RGB);
        return QImage(rgb.data, rgb.cols, rgb.rows, rgb.step, QImage::Format_RGB888).copy();
    }

    // 4 通道 BGRA -> QImage ARGB32
    if (mat.type() == CV_8UC4) {
        Mat rgba;
        cvtColor(mat, rgba, cv::COLOR_BGRA2RGBA);
        return QImage(rgba.data, rgba.cols, rgba.rows, rgba.step, QImage::Format_RGBA8888).copy();
    }

    // 其它不支持的类型
    qDebug() << "Unsupported Mat type in matToQImage(): " << mat.type();
    return QImage();
}

Mat ImageUtils::qImageToMat(const QImage &image, bool clone)
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


Mat ImageUtils::makeStructElement(int ksize)
{
    return getStructuringElement(MORPH_ELLIPSE,
                                 cv::Size(2*ksize+1, 2*ksize+1),
                                 cv::Point(ksize, ksize));
}

QRect ImageUtils::mapLabelToImage(const QRect &rect, const Mat &img, QLabel *label)
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

Mat ImageUtils::HImageToMat(HObject &H_img)
{
    cv::Mat cv_img;
    HalconCpp::HTuple channels, w, h;

    HalconCpp::ConvertImageType(H_img, &H_img, "byte");
    HalconCpp::CountChannels(H_img, &channels);

    if (channels.I() == 1)
    {
        HalconCpp::HTuple pointer;
        GetImagePointer1(H_img, &pointer, nullptr, &w, &h);
        int width = w.I(), height = h.I();
        int size = width * height;
        cv_img = cv::Mat::zeros(height, width, CV_8UC1);
        memcpy(cv_img.data, (void*)(pointer.L()), size);
        //std::cout << "Gray" << std::endl;
    }

    else if (channels.I() == 3)
    {
        HalconCpp::HTuple pointerR, pointerG, pointerB;
        HalconCpp::GetImagePointer3(H_img, &pointerR, &pointerG, &pointerB, nullptr, &w, &h);
        int width = w.I(), height = h.I();
        int size = width * height;
        cv_img = cv::Mat::zeros(height, width, CV_8UC3);
        uchar* R = (uchar*)(pointerR.L());
        uchar* G = (uchar*)(pointerG.L());
        uchar* B = (uchar*)(pointerB.L());
        for (int i = 0; i < height; ++i)
        {
            uchar *p = cv_img.ptr<uchar>(i);
            for (int j = 0; j < width; ++j)
            {
                p[3 * j] = B[i * width + j];
                p[3 * j + 1] = G[i * width + j];
                p[3 * j + 2] = R[i * width + j];
            }
        }
        //std::cout << "RGB" << std::endl;
    }
    return cv_img;
}

Mat ImageUtils::HObjectToMat(const HalconCpp::HObject& region, int width, int height)
{
    try {
        // Region -> Binary Image (byte)
        HalconCpp::HObject hBinImg;
        HalconCpp::RegionToBin(region, &hBinImg, 0, 255, width, height);

        // 取指针
        HalconCpp::HImage img(hBinImg);
        Hlong w, h;
        HalconCpp::HString type;
        unsigned char* ptr = (unsigned char*)img.GetImagePointer1(&type, &w, &h);

        // 拷回 Mat
        cv::Mat result((int)h, (int)w, CV_8UC1);
        memcpy(result.data, ptr, (size_t)w * (size_t)h);
        return result;
    }
    catch (HalconCpp::HException &ex) {
        qDebug() << "[HObjectToMat] Halcon异常:" << ex.ErrorMessage().Text();
        return cv::Mat::zeros(height, width, CV_8UC1);
    }
}


HalconCpp::HRegion ImageUtils::MatToHRegion(const cv::Mat &binary)
{
    using namespace HalconCpp;

    CV_Assert(!binary.empty());
    CV_Assert(binary.channels() == 1);

    cv::Mat bin8u;
    if (binary.type() != CV_8UC1) {
        binary.convertTo(bin8u, CV_8UC1);
    } else {
        bin8u = binary;
    }

    // ✅ 确保连续，否则 Halcon 按连续内存读会越界崩溃
    if (!bin8u.isContinuous()) {
        bin8u = bin8u.clone();
    }

    // ✅ 关键修改：取反，让编码规则与 HRegionToMat 一致
    // HRegionToMat 输出: Region内部=0, Region外部=255
    // 所以这里需要把 mask 中的 0 值区域（目标）变成 255，才能转为 Region
    cv::Mat inverted;
    cv::bitwise_not(bin8u, inverted);

    HImage hImg;
    // GenImage1 签名: GenImage1(const char* type, Hlong width, Hlong height, void* data)
    hImg.GenImage1("byte",
                   (Hlong)inverted.cols,
                   (Hlong)inverted.rows,
                   (void*)inverted.data);

    // ✅ 强烈建议：拷贝到 Halcon 内部内存，避免 Mat 生命周期/线程问题
    hImg = hImg.CopyImage();

    // Threshold 选择 [1, 255] 范围（取反后的非零值区域 = 原mask中的0值区域）
    return hImg.Threshold(1.0, 255.0);
}

Mat ImageUtils::HRegionToMat(const HRegion &region, int width, int height)
{
    // 检查输入参数
    if (width <= 0 || height <= 0)
    {
        return cv::Mat::zeros(1, 1, CV_8UC1);
    }

    try
    {
        // 检查 Region 是否为空
        HTuple area;
        area = region.Area();

        if (area.Length() == 0 || area[0].D() <= 0.0)
        {
            return cv::Mat(height, width, CV_8UC1, cv::Scalar(255));
        }

        // 1) Region -> Binary Image
        HImage binImg = region.RegionToBin(0, 255, width, height);

        // 2) Get pointer
        HalconCpp::HString type;
        Hlong w = 0, h = 0;
        unsigned char* ptr = (unsigned char*)binImg.GetImagePointer1(&type, &w, &h);

        if (ptr == nullptr || w <= 0 || h <= 0)
        {
            return cv::Mat::zeros(height, width, CV_8UC1);
        }

        // 3) Wrap -> clone
        cv::Mat mask((int)h, (int)w, CV_8UC1, ptr);

        if (mask.empty() || !mask.isContinuous())
        {
            return cv::Mat::zeros(height, width, CV_8UC1);
        }

        // 必须 clone，因为 ptr 的生命周期与 binImg 绑定
        return mask.clone();
    }
    catch (const HalconCpp::HException &ex)
    {
        qDebug() << "[HRegionToMat] Halcon异常:" << ex.ErrorMessage().Text();
        return cv::Mat::zeros(height, width, CV_8UC1);
    }
    catch (const cv::Exception &ex)
    {
        qDebug() << "[HRegionToMat] OpenCV异常:" << ex.what();
        return cv::Mat::zeros(height, width, CV_8UC1);
    }
    catch (...)
    {
        return cv::Mat::zeros(height, width, CV_8UC1);
    }
}


HImage ImageUtils::matToHImage(const cv::Mat& mat)
{
    if (mat.empty()) {
        throw std::runtime_error("matToHImage: 输入图像为空");
    }

    cv::Mat grayMat;

    // 1️⃣ 转换为灰度图
    if (mat.channels() == 3) {
        cv::cvtColor(mat, grayMat, cv::COLOR_BGR2GRAY);
    } else if (mat.channels() == 1) {
        grayMat = mat.clone();  // ✅ 确保拷贝一份
    } else {
        throw std::runtime_error("Mat2HImage: 不支持的通道数");
    }

    // 2️⃣ 确保数据连续
    if (!grayMat.isContinuous()) {
        grayMat = grayMat.clone();
    }

    // 3️⃣ 使用 GenImageInterleaved 创建图像（Halcon 会拷贝数据）
    HImage hImage;

    try {
        // ✅ 方法1：逐像素设置（安全但较慢）
        hImage.GenImage1("byte", grayMat.cols, grayMat.rows, (void*)grayMat.data);

        // Halcon 的 GenImage1 会立即读取数据，所以需要保证在这之前数据有效
        // 为了安全，我们创建一个临时拷贝

    } catch (const HException& ex) {
        throw std::runtime_error(
            QString("Mat2HImage 失败: %1").arg(ex.ErrorMessage().Text()).toStdString()
            );
    }

    return hImage;
}

