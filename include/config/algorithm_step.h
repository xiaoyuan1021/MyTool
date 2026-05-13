#pragma once

#include <QString>
#include <QMap>
#include <QVariant>

/**
 * @brief 算法步骤结构体（从 image_processor.h 提取）
 *
 * 独立定义，不依赖 OpenCV，供 PipelineManager、ConfigManager 等使用。
 */
struct AlgorithmStep
{
    QString name;                   // 算法显示名称
    QString type;                   // 算法类型标识
    QMap<QString, QVariant> params; // 参数字典
    bool enabled = true;            // 是否启用
    QString description;            // 算法说明
};