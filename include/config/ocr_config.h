#pragma once

#include <QJsonObject>
#include <QString>

/**
 * @brief OCR文字识别配置
 *
 * 基于 RapidOCR (PP-OCRv4) 引擎，支持中英文识别
 */
struct OcrConfig
{
    int maxSideLen = 960;                   // 图像最长边限制（缩放用）
    double confidenceThreshold = 0.3;       // 最小置信度阈值 (0~1)
    bool doAngle = true;                    // 是否启用文字方向检测
    int numThreads = 4;                     // 推理线程数

    bool operator==(const OcrConfig& o) const {
        return maxSideLen == o.maxSideLen &&
               confidenceThreshold == o.confidenceThreshold &&
               doAngle == o.doAngle &&
               numThreads == o.numThreads;
    }

    QJsonObject toJson() const {
        QJsonObject obj;
        obj["maxSideLen"] = maxSideLen;
        obj["confidenceThreshold"] = confidenceThreshold;
        obj["doAngle"] = doAngle;
        obj["numThreads"] = numThreads;
        return obj;
    }

    void fromJson(const QJsonObject& obj) {
        maxSideLen = obj["maxSideLen"].toInt(960);
        confidenceThreshold = obj["confidenceThreshold"].toDouble(0.3);
        doAngle = obj["doAngle"].toBool(true);
        numThreads = obj["numThreads"].toInt(4);
    }
};
