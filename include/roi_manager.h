#pragma once

#include <opencv2/opencv.hpp>
#include <QRectF>
#include <QString>

// 前向声明
class ImageView;
class QStatusBar;

/**
 * @class RoiManager
 * @brief ROI（感兴趣区域）管理器
 *
 * 职责：
 * - 管理完整图像和ROI裁剪后的图像
 * - 应用和重置ROI
 * - 跟踪ROI的激活状态和坐标
 * - 处理ROI相关的UI交互逻辑
 */
class RoiManager
{
public:
    RoiManager() = default;

    /**
     * @brief 设置完整图像
     * @param img 完整的图像数据
     */
    void setFullImage(const cv::Mat &img);

    /**
     * @brief 获取当前图像（ROI激活时返回ROI图像，否则返回完整图像）
     * @return 当前图像的引用
     */
    const cv::Mat &getCurrentImage() const;

    /**
     * @brief 获取完整图像
     * @return 完整图像的引用
     */
    const cv::Mat &getFullImage() const;

    /**
     * @brief 应用ROI区域
     * @param roiRectF ROI矩形（图像坐标系）
     * @return 是否应用成功
     */
    bool applyRoi(const QRectF &roiRectF);

    /**
     * @brief 重置ROI，恢复到完整图像
     */
    void resetRoi();

    /**
     * @brief 检查ROI是否激活
     * @return ROI激活状态
     */
    bool isRoiActive() const;

    /**
     * @brief 获取最后应用的ROI矩形
     * @return ROI矩形（OpenCV坐标系）
     */
    cv::Rect getLastRoi() const;

    /**
     * @brief 清空所有数据
     */
    void clear();

    // ========== UI交互相关方法 ==========

    /**
     * @brief 启用ROI绘制模式
     * @param view ImageView指针
     * @param statusBar 状态栏指针
     */
    void enableDrawRoiMode(ImageView *view, QStatusBar *statusBar);

    /**
     * @brief 处理ROI选择完成
     * @param roiImgRectF 选中的ROI矩形
     * @param statusBar 状态栏指针
     * @return 是否处理成功
     */
    bool handleRoiSelected(const QRectF &roiImgRectF, QStatusBar *statusBar);

    /**
     * @brief 重置ROI并更新UI
     * @param view ImageView指针
     * @param statusBar 状态栏指针
     */
    void resetRoiWithUI(ImageView *view, QStatusBar *statusBar);

private:
    cv::Mat m_fullImage;      // 完整图像
    cv::Mat m_roiImage;       // ROI裁剪后的图像
    bool m_isRoiActive = false; // ROI激活状态
    cv::Rect m_lastRoi;       // 最后应用的ROI矩形
};
