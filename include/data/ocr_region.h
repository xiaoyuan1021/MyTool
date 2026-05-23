#pragma once

#include <QString>

/**
 * @brief OCR文字识别区域
 */
struct OcrRegion
{
    int x = 0;              // 左上角x
    int y = 0;              // 左上角y
    int width = 0;          // 宽度
    int height = 0;         // 高度
    QString text;           // 识别的文字
    double confidence = 0;  // 置信度 (0~1)
};
