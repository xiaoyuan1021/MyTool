#pragma once

#include <QJsonObject>

/**
 * @brief 图像增强参数（原始滑块值版本）
 *
 * 用于 DetectionItem 各检测类型的增强参数统一定义。
 * 注意：与 PipelineConfig 中的 EnhanceConfig 不同——
 *   EnhanceConfig 使用归一化值（contrast=1.0 表示100%），
 *   EnhanceParams 使用滑块原始值（contrast=100 表示100%）。
 *
 * 设计目的：消除 BlobAnalysisConfig、LineDetectionConfig、
 * BarcodeRecognitionConfig、VideoSourceConfig 中的增强参数重复定义。
 * 每个 Config 持有独立的 EnhanceParams 实例，互不影响。
 */
struct EnhanceParams {
    int brightness = 0;    // 亮度 (-100 ~ 100)
    int contrast = 100;    // 对比度 (0 ~ 200)
    int gamma = 100;       // 伽马值 (10 ~ 300)
    int sharpen = 100;     // 锐化 (0 ~ 200)

    bool operator==(const EnhanceParams& o) const {
        return brightness == o.brightness &&
               contrast == o.contrast &&
               gamma == o.gamma &&
               sharpen == o.sharpen;
    }

    QJsonObject toJson() const {
        QJsonObject obj;
        obj["brightness"] = brightness;
        obj["contrast"] = contrast;
        obj["gamma"] = gamma;
        obj["sharpen"] = sharpen;
        return obj;
    }

    void fromJson(const QJsonObject& obj) {
        brightness = obj["brightness"].toInt(0);
        contrast = obj["contrast"].toInt(100);
        gamma = obj["gamma"].toInt(100);
        sharpen = obj["sharpen"].toInt(100);
    }
};