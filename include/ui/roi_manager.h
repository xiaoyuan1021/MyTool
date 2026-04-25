#pragma once

#include <opencv2/opencv.hpp>
#include <QRectF>
#include <QString>
#include <QMap>
#include <QVector>
#include "../config/roi_config.h"

// 前向声明
class ImageView;
class QStatusBar;

/**
 * @class RoiManager
 * @brief ROI（感兴趣区域）管理器（统一版）
 *
 * 职责：
 * - 管理完整图像和多个ROI配置（RoiConfig）
 * - 支持多图片，每张图片有自己的ROI配置列表
 * - 单ROI模式：通过 activeRoiId 指示当前生效的ROI
 * - 所有ROI数据统一存储在 roiConfigs 中
 */
class RoiManager : public QObject
{
    Q_OBJECT

public:
    RoiManager(QObject* parent = nullptr);

    // ==================== 图像管理 ====================

    void setFullImage(const cv::Mat &img);
    const cv::Mat &getFullImage() const;
    void clear();

    // ==================== 多图片管理 ====================

    QString addImage(const cv::Mat &img, const QString &name);
    QString addImage(const cv::Mat &img, const QString &name, const QString &filePath);
    bool switchToImage(const QString &imageId);
    bool removeImage(const QString &imageId);
    QString getCurrentImageId() const;
    QStringList getImageIds() const;
    QString getImageName(const QString &imageId) const;
    QString getImageFilePath(const QString &imageId) const;
    int imageCount() const;

    // ==================== 单ROI模式（基于 RoiConfig）====================

    /**
     * @brief 获取当前图像
     *        如果有激活的ROI，返回ROI裁剪后的图像；否则返回完整图像
     */
    cv::Mat getCurrentImage() const;

    /**
     * @brief 设置ROI区域并激活（单ROI模式兼容）
     *        如果该矩形对应的ROI已存在则激活它，否则创建新ROI并激活
     */
    bool setRoi(const QRectF &roiRectF);

    /**
     * @brief 重置ROI，恢复到完整图像（清除激活状态）
     */
    void resetRoi();

    /**
     * @brief 检查是否有ROI激活
     */
    bool isRoiActive() const;

    /**
     * @brief 获取最后应用的ROI矩形
     */
    cv::Rect getLastRoi() const;

    /**
     * @brief 获取ROI位置信息
     */
    cv::Rect getRoiPosition() const;

    // ==================== ROI配置管理（统一数据源）====================

    void addRoiConfig(const RoiConfig& config);
    bool removeRoiConfig(const QString& roiId);
    RoiConfig* getRoiConfig(const QString& roiId);
    const RoiConfig* getRoiConfig(const QString& roiId) const;
    QList<RoiConfig>& getRoiConfigs();
    const QList<RoiConfig>& getRoiConfigs() const;
    QList<RoiConfig> getRoiConfigsForImage(const QString& imageId) const;
    int getRoiConfigCount() const;
    void clearRoiConfigs();

    /// 生成下一个可用的默认ROI名称（基于现有ROI编号）
    QString generateNextRoiName() const;

    // ==================== 激活ROI管理 ====================

    void setActiveRoi(const QString& roiId);
    QString getActiveRoiId() const;
    void clearActiveRoi();

    // ==================== UI交互相关方法 ====================

    void enableDrawRoiMode(ImageView *view, QStatusBar *statusBar);
    bool handleRoiSelected(const QRectF &roiImgRectF, QStatusBar *statusBar);
    void resetRoiWithUI(ImageView *view, QStatusBar *statusBar);

    // ==================== 全量配置导出/导入 ====================

    QJsonDocument exportAllConfigsToJson() const;
    void importAllConfigsFromJson(const QJsonDocument& doc);

    // ==================== 缩放状态管理方法 ====================

    void saveZoomState(double scaleFactor, const QRectF& viewRect);
    bool getZoomState(const QString& imageId, double& scaleFactor, QRectF& viewRect) const;
    void clearZoomState(const QString& imageId);

signals:
    void roiConfigChanged();
    void imageAdded(const QString& imageId);
    void imageRemoved(const QString& imageId);
    void currentImageChanged(const QString& imageId);

private:
    // 多图片数据结构
    struct ImageRois {
        cv::Mat image;              // 图像数据
        QString name;               // 图片名称
        QString filePath;           // 图片文件路径
        QList<RoiConfig> roiConfigs;  // 该图片的ROI配置列表（统一数据源）
        QString activeRoiId;        // 当前激活的ROI ID（替代旧的单ROI模式）
        int roiCounter;             // ROI计数器

        // 缩放状态数据
        double scaleFactor;         // 缩放因子
        QRectF viewRect;            // 视图矩形
    };

    QMap<QString, ImageRois> m_imageRoisMap;  // 图片ID -> ROIs
    QString m_currentImageId;                  // 当前图片ID
    int m_imageCounter;                        // 图片计数器

    QString generateDefaultRoiName();
    QString generateDefaultImageName();

    // 辅助：根据矩形查找或创建ROI配置
    RoiConfig* findOrCreateRoiByRect(const QRectF& rect);
};
