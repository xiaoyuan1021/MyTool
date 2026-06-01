#pragma once

#include <QString>
#include <QJsonObject>
#include <QJsonDocument>
#include <QJsonArray>
#include <QVector>
#include <QVariantMap>
#include <QDateTime>
#include <QUuid>

/**
 * 单个检测项的上报结果
 */
struct DetectionItemReport
{
    QString itemName;           ///< 检测项名称
    QString detectionType;      ///< 检测类型（Blob/条码/直线/OCR/目标检测）
    bool passed = true;         ///< 是否通过
    QString failReason;         ///< 失败原因

    QJsonObject toJson() const {
        QJsonObject obj;
        obj["itemName"] = itemName;
        obj["detectionType"] = detectionType;
        obj["passed"] = passed;
        obj["failReason"] = failReason;
        return obj;
    }
};

/**
 * 检测结果上报数据结构
 * 
 * 用于标准化检测结果，支持 JSON 序列化，作为 MQTT 上报的数据载体。
 */
struct DetectionResultReport
{
    // ==================== 元数据 ====================
    QString reportId;           ///< 报告唯一ID
    QString imageId;            ///< 图片/帧ID
    QString imageName;          ///< 图片名称
    QString roiId;              ///< ROI ID（空字符串表示全图）
    QString roiName;            ///< ROI名称
    qint64 timestamp;           ///< 时间戳 (Unix ms)

    // ==================== 检测结果 ====================
    bool passed = true;         ///< 判定结果 (true=pass, false=fail)
    QString failReason;         ///< 失败原因描述

    // ==================== 检测项结果 ====================
    QVector<DetectionItemReport> itemResults;  ///< 各检测项的评估结果

    // ==================== 扩展字段 ====================
    QVariantMap customFields;   ///< 自定义扩展字段（预留）

    // ==================== 构造函数 ====================

    DetectionResultReport()
        : reportId(QUuid::createUuid().toString(QUuid::WithoutBraces))
        , timestamp(QDateTime::currentMSecsSinceEpoch())
    {
    }

    // ==================== JSON 序列化 ====================

    QJsonObject toJson() const
    {
        QJsonObject json;

        json["reportId"] = reportId;
        json["imageId"] = imageId;
        json["imageName"] = imageName;
        json["roiId"] = roiId;
        json["roiName"] = roiName;
        json["timestamp"] = timestamp;
        json["dateTime"] = QDateTime::fromMSecsSinceEpoch(timestamp).toString(Qt::ISODate);
        json["passed"] = passed;
        json["failReason"] = failReason;

        // 检测项结果
        QJsonArray itemsArray;
        for (const auto& item : itemResults) {
            itemsArray.append(item.toJson());
        }
        json["itemResults"] = itemsArray;

        // 统计摘要
        int passCount = 0, failCount = 0;
        for (const auto& item : itemResults) {
            if (item.passed) passCount++;
            else failCount++;
        }
        json["totalItems"] = itemResults.size();
        json["passedItems"] = passCount;
        json["failedItems"] = failCount;

        // 扩展字段
        QJsonObject customObj;
        for (auto it = customFields.constBegin(); it != customFields.constEnd(); ++it) {
            customObj[it.key()] = QJsonValue::fromVariant(it.value());
        }
        json["customFields"] = customObj;

        return json;
    }

    void fromJson(const QJsonObject& json)
    {
        reportId = json["reportId"].toString();
        imageId = json["imageId"].toString();
        imageName = json["imageName"].toString();
        roiId = json["roiId"].toString();
        roiName = json["roiName"].toString();
        timestamp = json["timestamp"].toVariant().toLongLong();
        passed = json["passed"].toBool(true);
        failReason = json["failReason"].toString();

        itemResults.clear();
        if (json.contains("itemResults")) {
            for (const auto& item : json["itemResults"].toArray()) {
                QJsonObject obj = item.toObject();
                DetectionItemReport ir;
                ir.itemName = obj["itemName"].toString();
                ir.detectionType = obj["detectionType"].toString();
                ir.passed = obj["passed"].toBool(true);
                ir.failReason = obj["failReason"].toString();
                itemResults.append(ir);
            }
        }

        customFields.clear();
        if (json.contains("customFields")) {
            QJsonObject customObj = json["customFields"].toObject();
            for (auto it = customObj.begin(); it != customObj.end(); ++it) {
                customFields[it.key()] = it.value().toVariant();
            }
        }
    }

    QString toJsonString() const
    {
        return QString::fromUtf8(QJsonDocument(toJson()).toJson(QJsonDocument::Compact));
    }

    bool fromJsonString(const QString& jsonString)
    {
        QJsonDocument doc = QJsonDocument::fromJson(jsonString.toUtf8());
        if (!doc.isObject()) return false;
        fromJson(doc.object());
        return true;
    }

    QString toSummaryString() const
    {
        int failCount = 0;
        for (const auto& item : itemResults) {
            if (!item.passed) failCount++;
        }
        QString summary = QString("[%1] %2 | %3/%4检测项通过")
            .arg(passed ? "PASS" : "FAIL")
            .arg(roiName.isEmpty() ? "全图" : roiName)
            .arg(itemResults.size() - failCount)
            .arg(itemResults.size());
        if (!passed && !failReason.isEmpty()) {
            summary += QString(" | 原因: %1").arg(failReason);
        }
        return summary;
    }
};
