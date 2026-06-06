#pragma once

#include <QObject>
#include <QString>
#include "roi_manager.h"
#include "config/detection_config_types.h"

/**
 * @brief 检测项配置更新控制器
 *
 * 负责更新当前选中ROI中各检测项的配置参数。
 * 从 RoiUiController 中提取，降低单一类的职责复杂度。
 *
 * 职责：
 * 1. 更新 Blob 检测项判定阈值
 * 2. 更新 OCR 检测项判定参数
 * 3. 更新目标检测配置
 * 4. 更新条码检测配置
 * 5. 更新直线检测配置
 */
class RoiDetectionConfigController : public QObject
{
    Q_OBJECT

public:
    explicit RoiDetectionConfigController(
        RoiManager& roiManager,
        QObject* parent = nullptr
    );

    /// 获取当前选中的ROI ID（由外部设置）
    void setCurrentRoiId(const QString& roiId) { m_currentRoiId = roiId; }
    QString currentRoiId() const { return m_currentRoiId; }

    /// 更新当前选中ROI的 Blob 检测项判定阈值（由 JudgeTab 触发）
    void updateBlobDetectionConfig(int minCount, int maxCount);

    /// 更新当前选中ROI的 OCR 检测项判定参数（由 OcrTab 触发）
    void updateOcrDetectionConfig(const QString& expectedText, bool matchExact);

    /// 更新当前选中ROI的 目标检测 检测项配置（由 ObjectDetectionTab 触发）
    void updateObjectDetectionConfig(const ObjectDetectionConfig& objConfig);

    /// 更新当前选中ROI的 条码检测 检测项配置（由 BarcodeTab 触发）
    void updateBarcodeDetectionConfig(const BarcodeConfig& barcodeConfig);

    /// 更新当前选中ROI的 直线检测 检测项配置（由 LineTab 触发）
    void updateLineDetectionConfig(const LineDetectConfig& lineConfig);

private:
    RoiManager& m_roiManager;
    QString m_currentRoiId;
};
