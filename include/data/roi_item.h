#pragma once

#include <opencv2/opencv.hpp>
#include <QRectF>
#include <QString>
#include <QUuid>
#include "pipeline.h"

/**
 * @class RoiItem
 * @brief 单个ROI项的封装
 *
 * 职责：
 * - 封装ROI的几何数据（矩形区域）
 * - 关联该ROI特定的检测配置
 * - 提供ROI的唯一标识和名称
 */
class RoiItem
{
public:
    /**
     * @brief 默认构造函数，生成唯一ID
     */
    RoiItem()
        : m_id(QUuid::createUuid().toString(QUuid::WithoutBraces))
        , m_name("ROI")
        , m_rect()
        , m_isValid(false)
    {
    }

    /**
     * @brief 带参数构造函数
     * @param name ROI名称
     * @param rect ROI矩形区域（OpenCV坐标系）
     */
    RoiItem(const QString &name, const cv::Rect &rect)
        : m_id(QUuid::createUuid().toString(QUuid::WithoutBraces))
        , m_name(name)
        , m_rect(rect)
        , m_isValid(rect.width > 0 && rect.height > 0)
    {
    }

    /**
     * @brief 从QRectF构造（图像坐标系）
     * @param name ROI名称
     * @param rectF ROI矩形（QRectF格式）
     */
    RoiItem(const QString &name, const QRectF &rectF)
        : m_id(QUuid::createUuid().toString(QUuid::WithoutBraces))
        , m_name(name)
        , m_isValid(false)
    {
        setRectFromQRectF(rectF);
    }

    // ========== Getter方法 ==========

    QString id() const { return m_id; }
    QString name() const { return m_name; }
    cv::Rect rect() const { return m_rect; }
    bool isValid() const { return m_isValid; }

    /**
     * @brief 获取ROI矩形（QRectF格式，用于UI）
     * @return QRectF格式的矩形
     */
    QRectF rectF() const
    {
        return QRectF(m_rect.x, m_rect.y, m_rect.width, m_rect.height);
    }

    /**
     * @brief 获取ROI关联的检测配置
     * @return 检测配置的常引用
     */
    const PipelineConfig& config() const { return m_config; }

    /**
     * @brief 获取ROI关联的检测配置（可修改）
     * @return 检测配置的引用
     */
    PipelineConfig& mutableConfig() { return m_config; }

    // ========== Setter方法 ==========

    void setName(const QString &name) { m_name = name; }

    void setRect(const cv::Rect &rect)
    {
        m_rect = rect;
        m_isValid = rect.width > 0 && rect.height > 0;
    }

    void setRectFromQRectF(const QRectF &rectF)
    {
        int x = (std::max)(0, (int)std::floor(rectF.x()));
        int y = (std::max)(0, (int)std::floor(rectF.y()));
        int w = (int)std::ceil(rectF.width());
        int h = (int)std::ceil(rectF.height());

        m_rect = cv::Rect(x, y, w, h);
        m_isValid = w > 0 && h > 0;
    }

    void setConfig(const PipelineConfig &config) { m_config = config; }

    // ========== 工具方法 ==========

    /**
     * @brief 检查点是否在ROI内
     * @param x 点的x坐标
     * @param y 点的y坐标
     * @return 是否在ROI内
     */
    bool contains(int x, int y) const
    {
        return m_isValid && m_rect.contains(cv::Point(x, y));
    }

    /**
     * @brief 从完整图像裁剪ROI区域
     * @param fullImage 完整图像
     * @return 裁剪后的ROI图像，如果无效则返回空Mat
     */
    cv::Mat cropFromImage(const cv::Mat &fullImage) const
    {
        if (!m_isValid || fullImage.empty()) {
            return cv::Mat();
        }

        // 手动计算ROI与图像边界的交集
        int x1 = (std::max)(m_rect.x, 0);
        int y1 = (std::max)(m_rect.y, 0);
        int x2 = (std::min)(m_rect.x + m_rect.width, fullImage.cols);
        int y2 = (std::min)(m_rect.y + m_rect.height, fullImage.rows);

        // 检查交集是否有效
        if (x1 >= x2 || y1 >= y2) {
            return cv::Mat();
        }

        cv::Rect validRect(x1, y1, x2 - x1, y2 - y1);
        return fullImage(validRect).clone();
    }

    /**
     * @brief 转换为字符串表示（用于调试）
     * @return 字符串
     */
    QString toString() const
    {
        return QString("RoiItem[id=%1, name=%2, rect=(%3,%4,%5,%6), valid=%7]")
            .arg(m_id)
            .arg(m_name)
            .arg(m_rect.x).arg(m_rect.y)
            .arg(m_rect.width).arg(m_rect.height)
            .arg(m_isValid ? "true" : "false");
    }

    /**
     * @brief 重置为默认配置
     */
    void resetConfig()
    {
        m_config = PipelineConfig();
    }

private:
    QString m_id;           ///< ROI唯一标识符
    QString m_name;         ///< ROI显示名称
    cv::Rect m_rect;        ///< ROI矩形区域（OpenCV坐标系）
    bool m_isValid;         ///< ROI是否有效
    PipelineConfig m_config; ///< 该ROI关联的检测配置
};