#pragma once

#include <opencv2/opencv.hpp>
#include <QRectF>
#include <QString>
#include <QMap>
#include <QVector>
#include "../data/roi_item.h"
#include "roi_config.h"

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
class RoiManager : public QObject
{
    Q_OBJECT

public:
    RoiManager(QObject* parent = nullptr);

    // ==================== 图像管理 ====================

    /**
     * @brief 设置完整图像（当前图片）
     * @param img 完整的图像数据
     */
    void setFullImage(const cv::Mat &img);

    /**
     * @brief 获取完整图像（当前图片）
     * @return 完整图像的引用
     */
    const cv::Mat &getFullImage() const;

    /**
     * @brief 清空所有数据
     */
    void clear();

    // ==================== 多图片管理 ====================

    /**
     * @brief 添加图片
     * @param img 图像数据
     * @param name 图片名称
     * @return 图片ID，失败返回空字符串
     */
    QString addImage(const cv::Mat &img, const QString &name);

    /**
     * @brief 添加图片（带文件路径）
     * @param img 图像数据
     * @param name 图片名称
     * @param filePath 图片文件路径
     * @return 图片ID，失败返回空字符串
     */
    QString addImage(const cv::Mat &img, const QString &name, const QString &filePath);

    /**
     * @brief 切换到指定图片
     * @param imageId 图片ID
     * @return 是否切换成功
     */
    bool switchToImage(const QString &imageId);

    /**
     * @brief 移除图片
     * @param imageId 图片ID
     * @return 是否移除成功
     */
    bool removeImage(const QString &imageId);

    /**
     * @brief 获取当前图片ID
     * @return 当前图片ID
     */
    QString getCurrentImageId() const;

    /**
     * @brief 获取所有图片ID
     * @return 图片ID列表
     */
    QStringList getImageIds() const;

    /**
     * @brief 获取图片名称
     * @param imageId 图片ID
     * @return 图片名称，如果图片不存在返回空字符串
     */
    QString getImageName(const QString &imageId) const;

    /**
     * @brief 获取图片文件路径
     * @param imageId 图片ID
     * @return 图片文件路径，如果不存在返回空字符串
     */
    QString getImageFilePath(const QString &imageId) const;

    /**
     * @brief 获取图片数量
     * @return 图片数量
     */
    int imageCount() const;

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

    // ==================== 全量配置导出/导入 ====================

    /**
     * @brief 导出所有图片的ROI配置为JSON
     * @return JSON文档
     */
    QJsonDocument exportAllConfigsToJson() const;

    /**
     * @brief 从JSON导入所有图片的ROI配置
     * @param doc JSON文档
     */
    void importAllConfigsFromJson(const QJsonDocument& doc);

    // ==================== 缩放状态管理方法 ====================

    /**
     * @brief 保存当前图片的缩放状态
     * @param scaleFactor 缩放因子
     * @param viewRect 视图矩形
     */
    void saveZoomState(double scaleFactor, const QRectF& viewRect);

    /**
     * @brief 获取指定图片的缩放状态
     * @param imageId 图片ID
     * @param scaleFactor 输出参数：缩放因子
     * @param viewRect 输出参数：视图矩形
     * @return 是否存在缩放状态
     */
    bool getZoomState(const QString& imageId, double& scaleFactor, QRectF& viewRect) const;

    /**
     * @brief 清除指定图片的缩放状态
     * @param imageId 图片ID
     */
    void clearZoomState(const QString& imageId);

    // ==================== ROI配置管理（替代MultiRoiConfig） ====================

    /**
     * @brief 添加ROI配置
     * @param config ROI配置
     */
    void addRoiConfig(const RoiConfig& config);

    /**
     * @brief 移除ROI配置
     * @param roiId ROI唯一ID
     * @return 是否移除成功
     */
    bool removeRoiConfig(const QString& roiId);

    /**
     * @brief 获取ROI配置（可修改）
     * @param roiId ROI唯一ID
     * @return RoiConfig指针，不存在返回nullptr
     */
    RoiConfig* getRoiConfig(const QString& roiId);

    /**
     * @brief 获取ROI配置（只读）
     * @param roiId ROI唯一ID
     * @return RoiConfig指针，不存在返回nullptr
     */
    const RoiConfig* getRoiConfig(const QString& roiId) const;

    /**
     * @brief 获取当前图片的所有ROI配置
     * @return ROI配置列表引用
     */
    QList<RoiConfig>& getRoiConfigs();
    const QList<RoiConfig>& getRoiConfigs() const;

    /**
     * @brief 获取指定图片的所有ROI配置（不切换当前图片）
     * @param imageId 图片ID
     * @return ROI配置列表的副本，如果图片不存在返回空列表
     */
    QList<RoiConfig> getRoiConfigsForImage(const QString& imageId) const;

    /**
     * @brief 获取当前图片的ROI配置数量
     */
    int getRoiConfigCount() const;

    /**
     * @brief 清空当前图片的所有ROI配置
     */
    void clearRoiConfigs();

    /**
     * @brief 序列化当前图片的ROI配置为JSON
     */
    QJsonDocument exportRoiConfigsToJson() const;

    /**
     * @brief 从JSON导入ROI配置到当前图片
     */
    void importRoiConfigsFromJson(const QJsonDocument& doc);

signals:
    // ROI配置变更信号
    void roiConfigChanged();

    // 多图片管理信号
    void imageAdded(const QString& imageId);
    void imageRemoved(const QString& imageId);
    void currentImageChanged(const QString& imageId);

private:
    // 多图片数据结构
    struct ImageRois {
        cv::Mat image;              // 图像数据
        QString name;               // 图片名称
        QString filePath;           // 图片文件路径
        QMap<QString, RoiItem> rois;  // 该图片的ROI列表（旧的多ROI）
        QList<RoiConfig> roiConfigs;  // 该图片的ROI配置列表（新的统一数据源）
        QString selectedRoiId;      // 该图片选中的ROI
        int roiCounter;             // ROI计数器
        
        // 单ROI模式数据
        bool isRoiActive;
        cv::Rect lastRoi;
        cv::Mat roiImage;
        
        // 缩放状态数据
        double scaleFactor;         // 缩放因子
        QRectF viewRect;            // 视图矩形
    };

    QMap<QString, ImageRois> m_imageRoisMap;  // 图片ID -> ROIs
    QString m_currentImageId;                  // 当前图片ID
    int m_imageCounter;                        // 图片计数器

    /**
     * @brief 生成默认ROI名称
     * @return 默认名称
     */
    QString generateDefaultRoiName();
    
    /**
     * @brief 生成默认图片名称
     * @return 默认名称
     */
    QString generateDefaultImageName();
};
