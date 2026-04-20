#ifndef ZXING_BARCODE_READER_H
#define ZXING_BARCODE_READER_H

#include <QString>
#include <QVector>
#include <QRectF>
#include <opencv2/core.hpp>

/**
 * ZXing条码识别结果结构
 */
struct ZXingBarcodeResult
{
    QString data;           // 条码数据
    QString type;           // 条码类型
    QRectF location;        // 位置信息
    int quality;            // 质量评分（-1表示不可用）
};

/**
 * ZXing条码读取器类
 * 使用ZXing-CPP库进行条码识别，支持QR Code、Data Matrix、EAN-13
 */
class ZXingBarcodeReader
{
public:
    ZXingBarcodeReader();
    ~ZXingBarcodeReader();

    /**
     * 识别图像中的条码
     * @param image 输入图像（OpenCV Mat格式，支持灰度或BGR）
     * @return 识别到的条码结果列表
     */
    QVector<ZXingBarcodeResult> readBarcodes(const cv::Mat& image);

    /**
     * 设置要识别的条码格式
     * @param formats 格式列表，如 {"QRCode", "DataMatrix", "EAN-13"}
     */
    void setFormats(const QStringList& formats);

    /**
     * 启用/禁用尝试旋转图像进行识别
     * @param enable 是否启用
     */
    void setTryRotate(bool enable);

    /**
     * 启用/禁用尝试反转图像（白底黑条码）
     * @param enable 是否启用
     */
    void setTryInvert(bool enable);

    /**
     * 启用/禁用尝试降采样
     * @param enable 是否启用
     */
    void setTryDownscale(bool enable);

private:
    bool tryRotate_;
    bool tryInvert_;
    bool tryDownscale_;
    QStringList formats_;

    /**
     * 将OpenCV Mat转换为ZXing可识别的格式并识别
     */
    QVector<ZXingBarcodeResult> decodeImage(const cv::Mat& gray);
};

#endif // ZXING_BARCODE_READER_H