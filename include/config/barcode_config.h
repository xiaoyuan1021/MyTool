#pragma once

#include <QJsonObject>
#include <QStringList>
#include "config/enhance_config.h"

/**
 * @brief 条码识别配置（统一版本）
 *
 * 合并了原 BarcodeConfig（PipelineConfig 用）和
 * BarcodeRecognitionConfig（DetectionItem 用），
 * 消除重复定义。两者字段完全一致。
 */
struct BarcodeConfig
{
    EnhanceConfig enhance;     ///< 图像增强参数（独立实例）

    // 条码识别参数
    bool enableBarcode = false;           // 是否启用条码识别
    QStringList codeTypes = {"auto"};     // 条码类型，"auto"表示自动检测
    int maxNumSymbols = 0;               // 最大识别数量，0=不限制
    bool returnQuality = true;           // 返回质量信息
    double minConfidence = 0.7;          // 最小置信度
    int timeout = 5000;                  // 超时时间（毫秒）

    // 图像预处理参数
    bool enablePreprocessing = true;     // 是否启用预处理
    int preprocessMethod = 0;            // 预处理方法 (0: 直接识别, 1: 二值化, 2: 形态学)
    int binarizationThreshold = 128;     // 二值化阈值
    int morphologySize = 3;              // 形态学核大小

    bool operator==(const BarcodeConfig& o) const {
        return enableBarcode == o.enableBarcode &&
               codeTypes == o.codeTypes &&
               maxNumSymbols == o.maxNumSymbols &&
               returnQuality == o.returnQuality;
    }

    QJsonObject toJson() const;
    void fromJson(const QJsonObject& obj);
};
