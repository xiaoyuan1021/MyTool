#pragma once

#include <QString>
#include <QVector>
#include <QJsonObject>
#include <QJsonArray>
#include "region_feature.h"
#include "barcode_result.h"
#include "ocr_region.h"

/**
 * 单个检测项的评估结果
 */
struct DetectionItemResult
{
    QString itemId;             ///< 检测项ID
    QString itemName;           ///< 检测项名称
    QString detectionType;      ///< 检测类型（Blob/Barcode/Line/OCR/ObjectDetection）
    bool passed = true;         ///< 判定结果
    QString failReason;         ///< 失败原因
    QJsonObject detail;         ///< 详细数据（可选）

    DetectionItemResult() = default;
    DetectionItemResult(const QString& id, const QString& name, const QString& type)
        : itemId(id), itemName(name), detectionType(type), passed(true) {}
};

/**
 * 单个ROI的检测结果
 * 
 * 核心设计：每个ROI的检测结果独立存储，不与其他ROI混合。
 * 这是解决"单独测试OK，批量检测NG"问题的关键数据结构。
 */
struct RoiDetectionResult
{
    // ==================== ROI标识 ====================
    QString roiId;              ///< ROI唯一ID
    QString roiName;            ///< ROI名称
    QString imageId;            ///< 所属图片ID

    // ==================== 判定结果 ====================
    bool passed = true;         ///< 该ROI整体判定（所有检测项都PASS才为true）
    QString failReason;         ///< 失败原因汇总

    // ==================== 图像处理结果 ====================
    int regionCount = 0;        ///< Blob区域数量
    std::vector<RegionFeature> regionFeatures;  ///< 区域特征

    // ==================== 条码识别结果 ====================
    QVector<BarcodeResult> barcodeResults;

    // ==================== OCR识别结果 ====================
    QString ocrText;            ///< 识别的文本
    QVector<OcrRegion> ocrRegions;

    // ==================== 直线检测结果 ====================
    int matchedLineCount = 0;   ///< 匹配的参考线数
    int totalLineCount = 0;     ///< 检测到的总直线数

    // ==================== 检测项评估结果 ====================
    QVector<DetectionItemResult> itemResults;

    // ==================== 构造函数 ====================
    RoiDetectionResult() = default;
    RoiDetectionResult(const QString& id, const QString& name, const QString& imgId)
        : roiId(id), roiName(name), imageId(imgId), passed(true) {}

    // ==================== 方法 ====================

    /**
     * 添加检测项评估结果
     */
    void addItemResult(const DetectionItemResult& result) {
        itemResults.append(result);
        if (!result.passed) {
            passed = false;
            if (!failReason.isEmpty()) failReason += "; ";
            failReason += QString("[%1] %2").arg(result.itemName, result.failReason);
        }
    }

    /**
     * 获取失败的检测项列表
     */
    QVector<DetectionItemResult> failedItems() const {
        QVector<DetectionItemResult> failed;
        for (const auto& item : itemResults) {
            if (!item.passed) {
                failed.append(item);
            }
        }
        return failed;
    }

    /**
     * 转换为JSON对象
     */
    QJsonObject toJson() const {
        QJsonObject json;
        json["roiId"] = roiId;
        json["roiName"] = roiName;
        json["imageId"] = imageId;
        json["passed"] = passed;
        json["failReason"] = failReason;
        json["regionCount"] = regionCount;
        json["matchedLineCount"] = matchedLineCount;
        json["totalLineCount"] = totalLineCount;
        json["ocrText"] = ocrText;

        // 检测项结果
        QJsonArray itemsArray;
        for (const auto& item : itemResults) {
            QJsonObject itemObj;
            itemObj["itemId"] = item.itemId;
            itemObj["itemName"] = item.itemName;
            itemObj["detectionType"] = item.detectionType;
            itemObj["passed"] = item.passed;
            itemObj["failReason"] = item.failReason;
            itemsArray.append(itemObj);
        }
        json["itemResults"] = itemsArray;

        // 条码结果
        QJsonArray barcodesArray;
        for (const auto& barcode : barcodeResults) {
            QJsonObject barcodeObj;
            barcodeObj["type"] = barcode.type;
            barcodeObj["data"] = barcode.data;
            barcodeObj["quality"] = barcode.quality;
            barcodesArray.append(barcodeObj);
        }
        json["barcodeResults"] = barcodesArray;

        return json;
    }

    /**
     * 生成简短摘要
     */
    QString toSummaryString() const {
        QString status = passed ? "PASS" : "FAIL";
        QString summary = QString("[%1] %2 | Blob:%3 | 条码:%4 | %5")
            .arg(status)
            .arg(roiName.isEmpty() ? "全图" : roiName)
            .arg(regionCount)
            .arg(barcodeResults.size())
            .arg(itemResults.isEmpty() ? "无检测项" : QString("%1个检测项").arg(itemResults.size()));
        
        if (!passed && !failReason.isEmpty()) {
            summary += QString(" | 原因: %1").arg(failReason);
        }
        return summary;
    }
};

/**
 * 图片级别的检测结果汇总
 * 
 * 包含一张图片所有ROI的检测结果，以及整体判定。
 */
struct ImageDetectionResult
{
    QString imageId;            ///< 图片ID
    QString imageName;          ///< 图片名称
    QString imagePath;          ///< 图片路径

    bool passed = true;         ///< 整体判定（所有ROI都PASS才为true）
    QString failReason;         ///< 失败原因汇总

    QVector<RoiDetectionResult> roiResults;  ///< 各ROI的检测结果

    // ==================== 方法 ====================

    /**
     * 添加ROI检测结果
     */
    void addRoiResult(const RoiDetectionResult& result) {
        roiResults.append(result);
        if (!result.passed) {
            passed = false;
            if (!failReason.isEmpty()) failReason += "\n";
            failReason += result.toSummaryString();
        }
    }

    /**
     * 获取失败的ROI列表
     */
    QVector<RoiDetectionResult> failedRois() const {
        QVector<RoiDetectionResult> failed;
        for (const auto& roi : roiResults) {
            if (!roi.passed) {
                failed.append(roi);
            }
        }
        return failed;
    }

    /**
     * 转换为JSON对象
     */
    QJsonObject toJson() const {
        QJsonObject json;
        json["imageId"] = imageId;
        json["imageName"] = imageName;
        json["imagePath"] = imagePath;
        json["passed"] = passed;
        json["failReason"] = failReason;

        QJsonArray roisArray;
        for (const auto& roi : roiResults) {
            roisArray.append(roi.toJson());
        }
        json["roiResults"] = roisArray;

        return json;
    }

    /**
     * 生成简短摘要
     */
    QString toSummaryString() const {
        int totalRois = roiResults.size();
        int passedRois = totalRois - failedRois().size();
        return QString("%1: %2/%3 ROI PASS | %4")
            .arg(imageName)
            .arg(passedRois)
            .arg(totalRois)
            .arg(passed ? "整体PASS" : "整体FAIL");
    }
};
