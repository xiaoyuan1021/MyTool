#pragma once

#include <QJsonObject>
#include <QString>

/**
 * @brief OCR文字识别配置
 *
 * 基于Tesseract OCR引擎，支持中英文识别
 */
struct OcrConfig
{
    QString language = "chi_sim+eng";    // 识别语言（chi_sim=中文，eng=英文）
    int pageMode = 0;                     // 页面分割模式（0=自动，1=单行，2=多行）
    int dpi = 0;                          // 图像分辨率（0=自动估计）
    double confidenceThreshold = 0.3;     // 最小置信度阈值 (0~1)
    QString whitelist = "";               // 字符白名单（空=不过滤）

    bool operator==(const OcrConfig& o) const {
        return language == o.language &&
               pageMode == o.pageMode &&
               dpi == o.dpi &&
               confidenceThreshold == o.confidenceThreshold &&
               whitelist == o.whitelist;
    }

    QJsonObject toJson() const {
        QJsonObject obj;
        obj["language"] = language;
        obj["pageMode"] = pageMode;
        obj["dpi"] = dpi;
        obj["confidenceThreshold"] = confidenceThreshold;
        obj["whitelist"] = whitelist;
        return obj;
    }

    void fromJson(const QJsonObject& obj) {
        language = obj["language"].toString("chi_sim+eng");
        pageMode = obj["pageMode"].toInt(0);
        dpi = obj["dpi"].toInt(0);
        confidenceThreshold = obj["confidenceThreshold"].toDouble(0.3);
        whitelist = obj["whitelist"].toString("");
    }
};
