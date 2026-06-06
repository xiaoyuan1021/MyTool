#pragma once

#include <QJsonObject>

/**
 * @brief 图像增强配置（亮度、对比度、伽马、锐化）
 *
 * 使用与 UI 滑块一致的整数原始值。
 * 在 Pipeline 执行时由 StepEnhance 内部转换为算法所需的比例值。
 *
 * 同时服务于 PipelineConfig（全局流水线）和
 * DetectionItem 各检测类型的独立增强参数。
 */
struct EnhanceConfig
{
    int brightness = 0;    // 亮度 (-100 ~ 100)
    int contrast = 100;    // 对比度 (0 ~ 300)，100 = 100%
    int gamma = 100;       // 伽马值 (10 ~ 300)，100 = 1.0
    int sharpen = 100;     // 锐化 (0 ~ 500)，100 = 1.0

    bool operator==(const EnhanceConfig& o) const {
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
