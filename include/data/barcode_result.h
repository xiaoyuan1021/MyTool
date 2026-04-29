#pragma once

#include <QString>
#include <QRectF>

/**
 * 条码识别结果
 */
struct BarcodeResult
{
    QString type;           // 条码类型 (如 "EAN-13", "QR Code")
    QString data;           // 解码数据
    QRectF location;        // 位置 (bounding box)
    double quality = 0.0;   // 识别质量 (0-100)
    int orientation = 0;    // 条码方向角度
};