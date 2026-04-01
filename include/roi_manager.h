#pragma once

#include <opencv2/opencv.hpp>
#include <QRectF>
#include <QString>
#include <QMap>
#include <QVector>
#include "roi_item.h"

// 前向声明
class ImageView;
class QStatusBar;

/**
 * @class RoiManager
 * @brief ROI（感兴趣区域）管理器
 *
 * 职责：
 * - 管理完整图像和多个ROI
 * - 支持传统单ROI模式（向后兼容）
 * - 支持多ROI模式（新增）
 * - 管理ROI与检测配置的关联
 */
class RoiManager
{
public:
    RoiManager() = default;

    // ==================== 图像管理 ====================

    /**
     * @brief 设置完整图像
     * @param img 完整的图像数据
     */
    void setFullImage(const cv::Mat &img);

    /**
     * @brief 获取完整图像
     * @return 完整图像的引用
     */
    const cv::Mat &getFullImage() const;

    /**
     * @brief 清空所有数据
     */
    void clear();

    // ==================== 单ROI模式（向后兼容） ====================

    /**
     * @brief 获取当前图像（单ROI模式：ROI激活时返回ROI图像，否则返回完整图像）
     * @return 当前图像的引用
     */
    const cv::Mat &getCurrentImage() const;

    /**
     * @brief 设置ROI区域（单ROI模式）
     * @param roiRectF ROI矩形（图像坐标系）
     * @return 是否设置成功
     */
    bool setRoi(const QRectF &roiRectF);

    /**
     * @brief 重置ROI，恢复到完整图像（单ROI模式）
     */
    void resetRoi();

    /**
     * @brief 检查ROI是否激活（单ROI模式）
     * @return ROI激活状态
     */
    bool isRoiActive() const;

    /**
     * @brief 获取最后应用的ROI矩形（单ROI模式）
     * @return ROI矩形（OpenCV坐标系）
     */
    cv::Rect getLastRoi() const;

    /**
     * @brief 获取ROI位置信息（单ROI模式）
     * @return ROI矩形（OpenCV坐标系），若ROI未激活则返回空矩形
     */
    cv::Rect getRoiPosition() const;

    // ==================== 多ROI模式（新增） ====================

    /**
     * @brief 添加一个新的ROI
     * @param name ROI名称
     * @param rect ROI矩形（图像坐标系）
     * @return 新创建的ROI的ID，失败返回空字符串
     */
    QString addRoi(const QString &name, const QRectF &rect);

    /**
     * @brief 添加一个已有的RoiItem
     * @param item RoiItem对象
     * @return 是否添加成功
     */
    bool addRoiItem(const RoiItem &item);

    /**
     * @brief 移除指定ID的ROI
     * @param roiId ROI的唯一标识
     * @return 是否移除成功
     */
    bool removeRoi(const QString &roiId);

    /**
     * @brief 更新指定ID的ROI矩形
     * @param roiId ROI的唯一标识
     * @param rect 新的矩形区域
     * @return 是否更新成功
     */
    bool updateRoiRect(const QString &roiId, const QRectF &rect);

    /**
     * @brief 更新指定ID的ROI名称
     * @param roiId ROI的唯一标识
     * @param name 新的名称
     * @return 是否更新成功
     */
    bool updateRoiName(const QString &roiId, const QString &name);

    /**
     * @brief 获取指定ID的ROI
     * @param roiId ROI的唯一标识
     * @return RoiItem指针，不存在返回nullptr
     */
    const RoiItem* getRoi(const QString &roiId) const;

    /**
     * @brief 获取指定ID的ROI（可修改）
     * @param roiId ROI的唯一标识
     * @return RoiItem指针，不存在返回nullptr
     */
    RoiItem* getMutableRoi(const QString &roiId);

    /**
     * @brief 获取所有ROI
     * @return ROI列表
     */
    QVector<RoiItem> getAllRois() const;

    /**
     * @brief 获取所有ROI的ID列表
     * @return ID列表
     */
    QStringList getRoiIds() const;

    /**
     * @brief 获取ROI数量
     * @return ROI数量
     */
    int roiCount() const;

    /**
     * @brief 检查是否存在指定ID的ROI
     * @param roiId ROI的唯一标识
     * @return 是否存在
     */
    bool hasRoi(const QString &roiId) const;

    /**
     * @brief 清空所有ROI（保留图像）
     */
    void clearAllRois();

    /**
     * @brief 设置选中的ROI
     * @param roiId ROI的唯一标识
     */
    void setSelectedRoi(const QString &roiId);

    /**
     * @brief 获取当前选中的ROI ID
     * @return ROI ID，未选中返回空字符串
     */
    QString getSelectedRoiId() const;

    /**
     * @brief 从完整图像裁剪指定ROI区域
     * @param roiId ROI的唯一标识
     * @return 裁剪后的图像，失败返回空Mat
     */
    cv::Mat cropRoi(const QString &roiId) const;

    // ==================== UI交互相关方法 ====================

    /**
     * @brief 启用ROI绘制模式
     * @param view ImageView指针
     * @param statusBar 状态栏指针
     */
    void enableDrawRoiMode(ImageView *view, QStatusBar *statusBar);

    /**
     * @brief 处理ROI选择完成（单ROI模式兼容）
     * @param roiImgRectF 选中的ROI矩形
     * @param statusBar 状态栏指针
     * @return 是否处理成功
     */
    bool handleRoiSelected(const QRectF &roiImgRectF, QStatusBar *statusBar);

    /**
     * @brief 处理多ROI选择完成
     * @param roiImgRectF 选中的ROI矩形
     * @param roiName ROI名称
     * @param statusBar 状态栏指针
     * @return 创建的ROI ID，失败返回空字符串
     */
    QString handleMultiRoiSelected(const QRectF &roiImgRectF, const QString &roiName, QStatusBar *statusBar);

    /**
     * @brief 重置ROI并更新UI（单ROI模式兼容）
     * @param view ImageView指针
     * @param statusBar 状态栏指针
     */
    void resetRoiWithUI(ImageView *view, QStatusBar *statusBar);

private:
    // 图像数据
    cv::Mat m_fullImage;      // 完整图像
    cv::Mat m_roiImage;       // ROI裁剪后的图像（单ROI模式兼容）

    // 单ROI模式数据（向后兼容）
    bool m_isRoiActive = false; // ROI激活状态
    cv::Rect m_lastRoi;       // 最后应用的ROI矩形

    // 多ROI模式数据
    QMap<QString, RoiItem> m_rois;  // ROI映射表（ID -> RoiItem）
    QString m_selectedRoiId;        // 当前选中的ROI ID
    int m_roiCounter = 0;           // ROI计数器（用于生成默认名称）

    /**
     * @brief 生成默认ROI名称
     * @return 默认名称
     */
    QString generateDefaultName();
};
