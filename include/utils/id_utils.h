#pragma once

#include <QString>
#include <QDateTime>
#include <QRandomGenerator>

/**
 * @brief 生成唯一ID
 * @param prefix 前缀（如 "roi" 或 "det"）
 * @return 唯一ID字符串
 */
inline QString generateUniqueId(const QString& prefix) {
    return QString("%1_%2_%3")
        .arg(prefix)
        .arg(QDateTime::currentMSecsSinceEpoch())
        .arg(QRandomGenerator::global()->bounded(10000));
}