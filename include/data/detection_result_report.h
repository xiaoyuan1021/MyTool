#pragma once

#include <QString>
#include <QJsonObject>
#include <QJsonDocument>
#include <QJsonArray>
#include <QVector>
#include <QVariantMap>
#include <QDateTime>
#include <QUuid>
#include "region_feature.h"
#include "pipeline.h"

/**
 * 检测结果上报数据结构
 * 
 * 用于标准化 Pipeline 执行结果，支持 JSON 序列化，
 * 作为 MQTT 上报的数据载体。
 */
struct DetectionResultReport
{
    // ==================== 元数据 ====================
    QString reportId;           ///< 报告唯一ID
    QString imageId;            ///< 图片/帧ID
    QString roiId;              ///< ROI ID（空字符串表示全图）
    QString roiName;            ///< ROI名称
    qint64 timestamp;           ///< 时间戳 (Unix ms)

    // ==================== 检测结果 ====================
    bool passed = true;         ///< 判定结果 (true=pass, false=fail)
    QString failReason;         ///< 失败原因描述

    // ==================== 区域统计 ====================
    int totalRegionCount = 0;           ///< 筛选后的区域总数
    int originalRegionCount = 0;        ///< 筛选前的原始区域数
    std::vector<RegionFeature> regions; ///< 区域特征详情

    // ==================== 条码识别结果 ====================
    std::vector<BarcodeResult> barcodeResults;  ///< 条码识别结果列表

    // ==================== 扩展字段 ====================
    QVariantMap customFields;   ///< 自定义扩展字段（预留）

    // ==================== 构造函数 ====================

    DetectionResultReport()
        : reportId(QUuid::createUuid().toString(QUuid::WithoutBraces))
        , timestamp(QDateTime::currentMSecsSinceEpoch())
    {
    }

    /**
     * 从 PipelineContext 快速构造报告
     * @param ctx Pipeline 执行上下文
     * @param imageId 图片ID
     * @param roiId ROI ID
     * @param roiName ROI名称
     */
    static DetectionResultReport fromPipelineContext(
        const PipelineContext& ctx,
        const QString& imageId = QString(),
        const QString& roiId = QString(),
        const QString& roiName = QString())
    {
        DetectionResultReport report;
        report.imageId = imageId;
        report.roiId = roiId;
        report.roiName = roiName;
        report.passed = ctx.pass;
        report.failReason = ctx.reason;
        report.totalRegionCount = ctx.currentRegions;
        report.regions = ctx.regions;
        report.barcodeResults = ctx.barcodeResults;
        return report;
    }

    // ==================== JSON 序列化 ====================

    /**
     * 转换为 JSON 对象
     */
    QJsonObject toJson() const
    {
        QJsonObject json;

        // 元数据
        json["reportId"] = reportId;
        json["imageId"] = imageId;
        json["roiId"] = roiId;
        json["roiName"] = roiName;
        json["timestamp"] = timestamp;
        json["dateTime"] = QDateTime::fromMSecsSinceEpoch(timestamp)
                              .toString(Qt::ISODate);

        // 检测结果
        QJsonObject resultObj;
        resultObj["passed"] = passed;
        resultObj["failReason"] = failReason;
        resultObj["totalRegionCount"] = totalRegionCount;
        resultObj["originalRegionCount"] = originalRegionCount;
        json["result"] = resultObj;

        // 区域特征
        QJsonArray regionsArray;
        for (const auto& region : regions) {
            QJsonObject regionObj;
            regionObj["index"] = region.index;
            regionObj["area"] = region.area;
            regionObj["circularity"] = region.circularity;
            regionObj["centerX"] = region.centerX;
            regionObj["centerY"] = region.centerY;
            regionObj["width"] = region.width;
            regionObj["height"] = region.height;
            regionObj["bbox"] = QJsonObject{
                {"x", region.bbox.x},
                {"y", region.bbox.y},
                {"width", region.bbox.width},
                {"height", region.bbox.height}
            };
            regionsArray.append(regionObj);
        }
        json["regions"] = regionsArray;

        // 条码结果
        QJsonArray barcodesArray;
        for (const auto& barcode : barcodeResults) {
            QJsonObject barcodeObj;
            barcodeObj["type"] = barcode.type;
            barcodeObj["data"] = barcode.data;
            barcodeObj["quality"] = barcode.quality;
            barcodeObj["orientation"] = barcode.orientation;
            QJsonObject locObj;
            locObj["x"] = barcode.location.x();
            locObj["y"] = barcode.location.y();
            locObj["width"] = barcode.location.width();
            locObj["height"] = barcode.location.height();
            barcodeObj["location"] = locObj;
            barcodesArray.append(barcodeObj);
        }
        json["barcodes"] = barcodesArray;

        // 扩展字段
        QJsonObject customObj;
        for (auto it = customFields.constBegin(); it != customFields.constEnd(); ++it) {
            customObj[it.key()] = QJsonValue::fromVariant(it.value());
        }
        json["customFields"] = customObj;

        return json;
    }

    /**
     * 从 JSON 对象解析
     */
    void fromJson(const QJsonObject& json)
    {
        // 元数据
        reportId = json["reportId"].toString(QUuid::createUuid().toString(QUuid::WithoutBraces));
        imageId = json["imageId"].toString();
        roiId = json["roiId"].toString();
        roiName = json["roiName"].toString();
        timestamp = json["timestamp"].toVariant().toLongLong();

        // 检测结果
        if (json.contains("result")) {
            QJsonObject resultObj = json["result"].toObject();
            passed = resultObj["passed"].toBool(true);
            failReason = resultObj["failReason"].toString();
            totalRegionCount = resultObj["totalRegionCount"].toInt(0);
            originalRegionCount = resultObj["originalRegionCount"].toInt(0);
        }

        // 区域特征
        regions.clear();
        if (json.contains("regions")) {
            QJsonArray regionsArray = json["regions"].toArray();
            for (const auto& item : regionsArray) {
                QJsonObject regionObj = item.toObject();
                RegionFeature feature;
                feature.index = regionObj["index"].toInt(0);
                feature.area = regionObj["area"].toDouble(0.0);
                feature.circularity = regionObj["circularity"].toDouble(0.0);
                feature.centerX = regionObj["centerX"].toDouble(0.0);
                feature.centerY = regionObj["centerY"].toDouble(0.0);
                feature.width = regionObj["width"].toDouble(0.0);
                feature.height = regionObj["height"].toDouble(0.0);
                if (regionObj.contains("bbox")) {
                    QJsonObject bboxObj = regionObj["bbox"].toObject();
                    feature.bbox = cv::Rect(
                        bboxObj["x"].toInt(0),
                        bboxObj["y"].toInt(0),
                        bboxObj["width"].toInt(0),
                        bboxObj["height"].toInt(0)
                    );
                }
                regions.push_back(feature);
            }
        }

        // 条码结果
        barcodeResults.clear();
        if (json.contains("barcodes")) {
            QJsonArray barcodesArray = json["barcodes"].toArray();
            for (const auto& item : barcodesArray) {
                QJsonObject barcodeObj = item.toObject();
                BarcodeResult barcode;
                barcode.type = barcodeObj["type"].toString();
                barcode.data = barcodeObj["data"].toString();
                barcode.quality = barcodeObj["quality"].toDouble(0.0);
                barcode.orientation = barcodeObj["orientation"].toInt(0);
                if (barcodeObj.contains("location")) {
                    QJsonObject locObj = barcodeObj["location"].toObject();
                    barcode.location = QRectF(
                        locObj["x"].toDouble(0),
                        locObj["y"].toDouble(0),
                        locObj["width"].toDouble(0),
                        locObj["height"].toDouble(0)
                    );
                }
                barcodeResults.push_back(barcode);
            }
        }

        // 扩展字段
        customFields.clear();
        if (json.contains("customFields")) {
            QJsonObject customObj = json["customFields"].toObject();
            for (auto it = customObj.begin(); it != customObj.end(); ++it) {
                customFields[it.key()] = it.value().toVariant();
            }
        }
    }

    /**
     * 序列化为 JSON 字符串
     */
    QString toJsonString() const
    {
        QJsonDocument doc(toJson());
        return QString::fromUtf8(doc.toJson(QJsonDocument::Compact));
    }

    /**
     * 从 JSON 字符串解析
     */
    bool fromJsonString(const QString& jsonString)
    {
        QJsonDocument doc = QJsonDocument::fromJson(jsonString.toUtf8());
        if (!doc.isObject()) {
            return false;
        }
        fromJson(doc.object());
        return true;
    }

    /**
     * 生成简短的结果摘要（用于日志/调试）
     */
    QString toSummaryString() const
    {
        QString barcodeInfo = barcodeResults.empty()
            ? "无条码"
            : QString("条码数: %1").arg(static_cast<int>(barcodeResults.size()));
        QString summary = QString("[%1] %2 | %3 | %4")
            .arg(passed ? "PASS" : "FAIL")
            .arg(roiName.isEmpty() ? "全图" : roiName)
            .arg(QString("区域数: %1").arg(totalRegionCount))
            .arg(barcodeInfo);
        
        if (!passed && !failReason.isEmpty()) {
            summary += QString(" | 原因: %1").arg(failReason);
        }
        return summary;
    }
};