#pragma once

#include <QJsonObject>
#include <QString>

/**
 * @brief 目标检测配置参数
 */
struct ObjectDetectionConfig {
    // 模型参数
    QString modelPath = "";                 // 模型文件路径
    QString configPath = "";                // 配置文件路径（可选）

    // 检测参数
    float confidenceThreshold = 0.5f;       // 置信度阈值 (0.0 ~ 1.0)
    float nmsThreshold = 0.4f;              // 非极大值抑制阈值 (0.0 ~ 1.0)
    int inputWidth = 640;                    // 输入宽度
    int inputHeight = 640;                   // 输入高度

    // 显示参数
    bool showLabels = true;                  // 是否显示标签
    bool showConfidence = true;              // 是否显示置信度
    bool showBoundingBox = true;             // 是否显示边框
    int lineWidth = 2;                       // 边框线宽

    // 判定参数
    int expectedCount = 0;                   // 期望检测数量（0=不检查数量）

    // JSON 序列化
    QJsonObject toJson() const {
        QJsonObject obj;
        obj["modelPath"] = modelPath;
        obj["configPath"] = configPath;
        obj["confidenceThreshold"] = static_cast<double>(confidenceThreshold);
        obj["nmsThreshold"] = static_cast<double>(nmsThreshold);
        obj["inputWidth"] = inputWidth;
        obj["inputHeight"] = inputHeight;
        obj["showLabels"] = showLabels;
        obj["showConfidence"] = showConfidence;
        obj["showBoundingBox"] = showBoundingBox;
        obj["lineWidth"] = lineWidth;
        obj["expectedCount"] = expectedCount;
        return obj;
    }

    void fromJson(const QJsonObject& obj) {
        modelPath = obj["modelPath"].toString();
        configPath = obj["configPath"].toString();
        confidenceThreshold = static_cast<float>(obj["confidenceThreshold"].toDouble(0.5));
        nmsThreshold = static_cast<float>(obj["nmsThreshold"].toDouble(0.4));
        inputWidth = obj["inputWidth"].toInt(640);
        inputHeight = obj["inputHeight"].toInt(640);
        showLabels = obj["showLabels"].toBool(true);
        showConfidence = obj["showConfidence"].toBool(true);
        showBoundingBox = obj["showBoundingBox"].toBool(true);
        lineWidth = obj["lineWidth"].toInt(2);
        expectedCount = obj["expectedCount"].toInt(0);
    }
};
