#include "roi_manager.h"
#include "logger.h"
#include "image_view.h"
#include "../data/roi_item.h"
#include <QStatusBar>
#include <QDebug>
#include <algorithm>

// ==================== 构造函数 ====================

RoiManager::RoiManager(QObject* parent)
    : QObject(parent)
    , m_imageCounter(0)
{
}

// ==================== 图像管理 ====================

void RoiManager::setFullImage(const cv::Mat &img)
{
    // 如果没有当前图片，添加到图片管理器
    if (m_currentImageId.isEmpty()) {
        QString imageId = addImage(img, generateDefaultImageName());
        switchToImage(imageId);
    } else {
        // 更新当前图片
        auto it = m_imageRoisMap.find(m_currentImageId);
        if (it != m_imageRoisMap.end()) {
            it.value().image = img.clone();
            it.value().isRoiActive = false;
            it.value().roiImage.release();
        }
    }
}

const cv::Mat& RoiManager::getFullImage() const
{
    auto it = m_imageRoisMap.find(m_currentImageId);
    if (it != m_imageRoisMap.end()) {
        return it.value().image;
    }
    
    // 返回空图像作为后备
    static cv::Mat emptyImage;
    return emptyImage;
}

void RoiManager::clear()
{
    m_imageRoisMap.clear();
    m_currentImageId.clear();
    m_imageCounter = 0;
}

// ==================== 多图片管理 ====================

QString RoiManager::addImage(const cv::Mat &img, const QString &name)
{
    if (img.empty()) {
        qDebug() << "[RoiManager] 图像为空，无法添加";
        return QString();
    }

    // 生成图片ID
    QString imageId = QString("img_%1_%2")
        .arg(QDateTime::currentMSecsSinceEpoch())
        .arg(m_imageCounter++);

    // 创建图片ROI数据
    ImageRois imageRois;
    imageRois.image = img.clone();
    imageRois.name = name.isEmpty() ? generateDefaultImageName() : name;
    imageRois.selectedRoiId.clear();
    imageRois.roiCounter = 0;
    imageRois.isRoiActive = false;
    imageRois.lastRoi = cv::Rect();
    imageRois.roiImage.release();
    // 初始化缩放状态
    imageRois.scaleFactor = 1.0;
    imageRois.viewRect = QRectF(0, 0, img.cols, img.rows);

    m_imageRoisMap.insert(imageId, imageRois);

    Logger::instance()->info(QString("[RoiManager] 图片已添加: id=%1, name=%2")
                                 .arg(imageId).arg(imageRois.name));

    emit imageAdded(imageId);
    return imageId;
}

bool RoiManager::switchToImage(const QString &imageId)
{
    if (!m_imageRoisMap.contains(imageId)) {
        qDebug() << "[RoiManager] 图片不存在:" << imageId;
        return false;
    }

    m_currentImageId = imageId;
    
    // 触发信号
    emit currentImageChanged(imageId);
    
    Logger::instance()->info(QString("[RoiManager] 切换到图片: id=%1").arg(imageId));
    return true;
}

bool RoiManager::removeImage(const QString &imageId)
{
    if (!m_imageRoisMap.contains(imageId)) {
        qDebug() << "[RoiManager] 图片不存在:" << imageId;
        return false;
    }

    m_imageRoisMap.remove(imageId);

    // 如果删除的是当前图片，切换到其他图片
    if (m_currentImageId == imageId) {
        if (!m_imageRoisMap.isEmpty()) {
            m_currentImageId = m_imageRoisMap.firstKey();
            emit currentImageChanged(m_currentImageId);
        } else {
            m_currentImageId.clear();
        }
    }

    Logger::instance()->info(QString("[RoiManager] 图片已移除: id=%1").arg(imageId));
    emit imageRemoved(imageId);
    
    return true;
}

QString RoiManager::getCurrentImageId() const
{
    return m_currentImageId;
}

QStringList RoiManager::getImageIds() const
{
    return m_imageRoisMap.keys();
}

QString RoiManager::getImageName(const QString &imageId) const
{
    auto it = m_imageRoisMap.find(imageId);
    if (it != m_imageRoisMap.end()) {
        return it.value().name;
    }
    return QString();
}

int RoiManager::imageCount() const
{
    return m_imageRoisMap.size();
}

// ==================== 单ROI模式（向后兼容） ====================

const cv::Mat& RoiManager::getCurrentImage() const
{
    auto it = m_imageRoisMap.find(m_currentImageId);
    if (it != m_imageRoisMap.end()) {
        return it.value().isRoiActive ? it.value().roiImage : it.value().image;
    }
    
    static cv::Mat emptyImage;
    return emptyImage;
}

bool RoiManager::setRoi(const QRectF &roiRectF)
{
    auto it = m_imageRoisMap.find(m_currentImageId);
    if (it == m_imageRoisMap.end()) {
        qDebug() << "[RoiManager] 没有当前图片";
        return false;
    }

    ImageRois& imageRois = it.value();
    
    if (imageRois.image.empty()) {
        qDebug() << "[RoiManager] 完整图像为空";
        return false;
    }

    // 转换并验证ROI区域
    int x = std::max(0, (int)std::floor(roiRectF.x()));
    int y = std::max(0, (int)std::floor(roiRectF.y()));
    int w = std::min((int)std::ceil(roiRectF.width()),
                     imageRois.image.cols - x);
    int h = std::min((int)std::ceil(roiRectF.height()),
                     imageRois.image.rows - y);

    if (w <= 0 || h <= 0) {
        qDebug() << "[RoiManager] ROI区域无效";
        return false;
    }

    // 裁剪ROI
    cv::Rect roi(x, y, w, h);
    imageRois.roiImage = imageRois.image(roi).clone();
    imageRois.isRoiActive = true;
    imageRois.lastRoi = roi;

    Logger::instance()->info(QString("[RoiManager] ROI已应用: x=%1 y=%2 w=%3 h=%4")
                                 .arg(x).arg(y).arg(w).arg(h));

    return true;
}

void RoiManager::resetRoi()
{
    auto it = m_imageRoisMap.find(m_currentImageId);
    if (it == m_imageRoisMap.end()) {
        return;
    }

    ImageRois& imageRois = it.value();
    imageRois.roiImage.release();
    imageRois.isRoiActive = false;
    
    Logger::instance()->info("[RoiManager] ROI已重置");
}

bool RoiManager::isRoiActive() const
{
    auto it = m_imageRoisMap.find(m_currentImageId);
    if (it == m_imageRoisMap.end()) {
        return false;
    }
    return it.value().isRoiActive;
}

cv::Rect RoiManager::getLastRoi() const
{
    auto it = m_imageRoisMap.find(m_currentImageId);
    if (it == m_imageRoisMap.end()) {
        return cv::Rect();
    }
    return it.value().lastRoi;
}

cv::Rect RoiManager::getRoiPosition() const
{
    auto it = m_imageRoisMap.find(m_currentImageId);
    if (it == m_imageRoisMap.end() || !it.value().isRoiActive) {
        return cv::Rect();
    }
    return it.value().lastRoi;
}

// ==================== 多ROI模式（新增） ====================

QString RoiManager::addRoi(const QString &name, const QRectF &rect)
{
    auto it = m_imageRoisMap.find(m_currentImageId);
    if (it == m_imageRoisMap.end()) {
        qDebug() << "[RoiManager] 没有当前图片，无法添加ROI";
        return QString();
    }

    ImageRois& imageRois = it.value();
    
    if (imageRois.image.empty()) {
        qDebug() << "[RoiManager] 完整图像为空，无法添加ROI";
        return QString();
    }

    RoiItem item(name.isEmpty() ? generateDefaultRoiName() : name, rect);
    if (!item.isValid()) {
        qDebug() << "[RoiManager] ROI区域无效";
        return QString();
    }

    // 确保ROI在图像范围内
    cv::Rect roiRect = item.rect();
    if (roiRect.x >= imageRois.image.cols || roiRect.y >= imageRois.image.rows) {
        qDebug() << "[RoiManager] ROI超出图像范围";
        return QString();
    }

    // 调整ROI大小以适应图像
    int w = std::min(roiRect.width, imageRois.image.cols - roiRect.x);
    int h = std::min(roiRect.height, imageRois.image.rows - roiRect.y);
    item.setRect(cv::Rect(roiRect.x, roiRect.y, w, h));

    QString roiId = item.id();
    imageRois.rois.insert(roiId, item);

    Logger::instance()->info(QString("[RoiManager] 多ROI已添加: id=%1, name=%2, rect=(%3,%4,%5,%6)")
                                 .arg(roiId).arg(name)
                                 .arg(item.rect().x).arg(item.rect().y)
                                 .arg(item.rect().width).arg(item.rect().height));

    return roiId;
}

bool RoiManager::addRoiItem(const RoiItem &item)
{
    auto it = m_imageRoisMap.find(m_currentImageId);
    if (it == m_imageRoisMap.end()) {
        qDebug() << "[RoiManager] 没有当前图片";
        return false;
    }

    if (!item.isValid()) {
        qDebug() << "[RoiManager] RoiItem无效";
        return false;
    }

    QString roiId = item.id();
    ImageRois& imageRois = it.value();
    
    if (imageRois.rois.contains(roiId)) {
        qDebug() << "[RoiManager] ROI ID已存在:" << roiId;
        return false;
    }

    imageRois.rois.insert(roiId, item);

    Logger::instance()->info(QString("[RoiManager] RoiItem已添加: %1").arg(item.toString()));

    return true;
}

bool RoiManager::removeRoi(const QString &roiId)
{
    auto it = m_imageRoisMap.find(m_currentImageId);
    if (it == m_imageRoisMap.end()) {
        qDebug() << "[RoiManager] 没有当前图片";
        return false;
    }

    ImageRois& imageRois = it.value();
    
    if (!imageRois.rois.contains(roiId)) {
        qDebug() << "[RoiManager] ROI不存在:" << roiId;
        return false;
    }

    imageRois.rois.remove(roiId);

    // 如果删除的是选中的ROI，清除选中状态
    if (imageRois.selectedRoiId == roiId) {
        imageRois.selectedRoiId.clear();
    }

    Logger::instance()->info(QString("[RoiManager] ROI已移除: id=%1").arg(roiId));

    return true;
}

bool RoiManager::updateRoiRect(const QString &roiId, const QRectF &rect)
{
    auto it = m_imageRoisMap.find(m_currentImageId);
    if (it == m_imageRoisMap.end()) {
        qDebug() << "[RoiManager] 没有当前图片";
        return false;
    }

    ImageRois& imageRois = it.value();
    
    if (!imageRois.rois.contains(roiId)) {
        qDebug() << "[RoiManager] ROI不存在:" << roiId;
        return false;
    }

    RoiItem &item = imageRois.rois[roiId];
    item.setRectFromQRectF(rect);

    if (!item.isValid()) {
        qDebug() << "[RoiManager] 更新的ROI区域无效";
        return false;
    }

    Logger::instance()->info(QString("[RoiManager] ROI矩形已更新: id=%1, rect=(%2,%3,%4,%5)")
                                 .arg(roiId)
                                 .arg(item.rect().x).arg(item.rect().y)
                                 .arg(item.rect().width).arg(item.rect().height));

    return true;
}

bool RoiManager::updateRoiName(const QString &roiId, const QString &name)
{
    auto it = m_imageRoisMap.find(m_currentImageId);
    if (it == m_imageRoisMap.end()) {
        qDebug() << "[RoiManager] 没有当前图片";
        return false;
    }

    ImageRois& imageRois = it.value();
    
    if (!imageRois.rois.contains(roiId)) {
        qDebug() << "[RoiManager] ROI不存在:" << roiId;
        return false;
    }

    imageRois.rois[roiId].setName(name);

    Logger::instance()->info(QString("[RoiManager] ROI名称已更新: id=%1, name=%2")
                                 .arg(roiId).arg(name));

    return true;
}

const RoiItem* RoiManager::getRoi(const QString &roiId) const
{
    auto it = m_imageRoisMap.find(m_currentImageId);
    if (it == m_imageRoisMap.end()) {
        return nullptr;
    }

    const ImageRois& imageRois = it.value();
    auto roiIt = imageRois.rois.find(roiId);
    if (roiIt == imageRois.rois.end()) {
        return nullptr;
    }
    return &(*roiIt);
}

RoiItem* RoiManager::getMutableRoi(const QString &roiId)
{
    auto it = m_imageRoisMap.find(m_currentImageId);
    if (it == m_imageRoisMap.end()) {
        return nullptr;
    }

    ImageRois& imageRois = it.value();
    auto roiIt = imageRois.rois.find(roiId);
    if (roiIt == imageRois.rois.end()) {
        return nullptr;
    }
    return &(*roiIt);
}

QVector<RoiItem> RoiManager::getAllRois() const
{
    auto it = m_imageRoisMap.find(m_currentImageId);
    if (it == m_imageRoisMap.end()) {
        return QVector<RoiItem>();
    }

    const ImageRois& imageRois = it.value();
    return imageRois.rois.values().toVector();
}

QStringList RoiManager::getRoiIds() const
{
    auto it = m_imageRoisMap.find(m_currentImageId);
    if (it == m_imageRoisMap.end()) {
        return QStringList();
    }

    const ImageRois& imageRois = it.value();
    return imageRois.rois.keys();
}

int RoiManager::roiCount() const
{
    auto it = m_imageRoisMap.find(m_currentImageId);
    if (it == m_imageRoisMap.end()) {
        return 0;
    }

    const ImageRois& imageRois = it.value();
    return imageRois.rois.size();
}

bool RoiManager::hasRoi(const QString &roiId) const
{
    auto it = m_imageRoisMap.find(m_currentImageId);
    if (it == m_imageRoisMap.end()) {
        return false;
    }

    const ImageRois& imageRois = it.value();
    return imageRois.rois.contains(roiId);
}

void RoiManager::clearAllRois()
{
    auto it = m_imageRoisMap.find(m_currentImageId);
    if (it == m_imageRoisMap.end()) {
        return;
    }

    ImageRois& imageRois = it.value();
    imageRois.rois.clear();
    imageRois.selectedRoiId.clear();
    imageRois.roiCounter = 0;

    Logger::instance()->info("[RoiManager] 所有ROI已清空");
}

void RoiManager::setSelectedRoi(const QString &roiId)
{
    auto it = m_imageRoisMap.find(m_currentImageId);
    if (it == m_imageRoisMap.end()) {
        return;
    }

    ImageRois& imageRois = it.value();
    
    if (roiId.isEmpty() || imageRois.rois.contains(roiId)) {
        imageRois.selectedRoiId = roiId;

        if (!roiId.isEmpty()) {
            Logger::instance()->info(QString("[RoiManager] ROI已选中: id=%1").arg(roiId));
        }
    }
}

QString RoiManager::getSelectedRoiId() const
{
    auto it = m_imageRoisMap.find(m_currentImageId);
    if (it == m_imageRoisMap.end()) {
        return QString();
    }

    const ImageRois& imageRois = it.value();
    return imageRois.selectedRoiId;
}

cv::Mat RoiManager::cropRoi(const QString &roiId) const
{
    auto it = m_imageRoisMap.find(m_currentImageId);
    if (it == m_imageRoisMap.end()) {
        qDebug() << "[RoiManager] 没有当前图片";
        return cv::Mat();
    }

    const ImageRois& imageRois = it.value();
    
    if (!imageRois.rois.contains(roiId)) {
        qDebug() << "[RoiManager] ROI不存在:" << roiId;
        return cv::Mat();
    }

    const RoiItem &item = imageRois.rois[roiId];
    return item.cropFromImage(imageRois.image);
}

QString RoiManager::generateDefaultRoiName()
{
    auto it = m_imageRoisMap.find(m_currentImageId);
    if (it == m_imageRoisMap.end()) {
        return QString("ROI_001");
    }

    ImageRois& imageRois = it.value();
    imageRois.roiCounter++;
    return QString("ROI_%1").arg(imageRois.roiCounter, 3, 10, QChar('0'));
}

QString RoiManager::generateDefaultImageName()
{
    return QString("图片_%1").arg(m_imageCounter + 1);
}

// ==================== UI交互相关方法 ====================

void RoiManager::enableDrawRoiMode(ImageView *view, QStatusBar *statusBar)
{
    auto it = m_imageRoisMap.find(m_currentImageId);
    if (it == m_imageRoisMap.end() || it.value().image.empty()) {
        return;
    }

    view->setRoiMode(true);
    view->setDragMode(QGraphicsView::NoDrag);
    if (statusBar) {
        statusBar->showMessage("请按下左键绘制ROI");
    }
}

bool RoiManager::handleRoiSelected(const QRectF &roiImgRectF, QStatusBar *statusBar)
{
    if (!setRoi(roiImgRectF)) {
        if (statusBar) {
            statusBar->showMessage("ROI应用失败", 2000);
        }
        return false;
    }

    cv::Rect roi = getLastRoi();
    if (statusBar) {
        statusBar->showMessage(
            QString("ROI已选择：x=%1 y=%2 w=%3 h=%4")
                .arg(roi.x).arg(roi.y).arg(roi.width).arg(roi.height),
            2000
        );
    }

    return true;
}

QString RoiManager::handleMultiRoiSelected(const QRectF &roiImgRectF, const QString &roiName, QStatusBar *statusBar)
{
    QString roiId = addRoi(roiName, roiImgRectF);

    if (roiId.isEmpty()) {
        if (statusBar) {
            statusBar->showMessage("多ROI添加失败", 2000);
        }
        return QString();
    }

    const RoiItem *item = getRoi(roiId);
    if (item && statusBar) {
        cv::Rect roi = item->rect();
        statusBar->showMessage(
            QString("ROI已添加：%1 (x=%2 y=%3 w=%4 h=%5)")
                .arg(item->name())
                .arg(roi.x).arg(roi.y).arg(roi.width).arg(roi.height),
            2000
        );
    }

    return roiId;
}

void RoiManager::resetRoiWithUI(ImageView *view, QStatusBar *statusBar)
{
    auto it = m_imageRoisMap.find(m_currentImageId);
    if (it == m_imageRoisMap.end() || it.value().image.empty()) {
        return;
    }

    resetRoi();
    view->clearRoi();

    if (statusBar) {
        statusBar->showMessage("ROI已重置，处理使用完整图像", 2000);
    }
    Logger::instance()->info("ROI已重置");
}

// ==================== 缩放状态管理方法实现 ====================

void RoiManager::saveZoomState(double scaleFactor, const QRectF& viewRect)
{
    auto it = m_imageRoisMap.find(m_currentImageId);
    if (it == m_imageRoisMap.end()) {
        return;
    }

    it.value().scaleFactor = scaleFactor;
    it.value().viewRect = viewRect;
    
    Logger::instance()->info(QString("[RoiManager] 保存缩放状态: scale=%1, viewRect=(%2,%3,%4,%5)")
                                 .arg(scaleFactor)
                                 .arg(viewRect.x()).arg(viewRect.y())
                                 .arg(viewRect.width()).arg(viewRect.height()));
}

bool RoiManager::getZoomState(const QString& imageId, double& scaleFactor, QRectF& viewRect) const
{
    auto it = m_imageRoisMap.find(imageId);
    if (it == m_imageRoisMap.end()) {
        return false;
    }

    scaleFactor = it.value().scaleFactor;
    viewRect = it.value().viewRect;
    return true;
}

void RoiManager::clearZoomState(const QString& imageId)
{
    auto it = m_imageRoisMap.find(imageId);
    if (it == m_imageRoisMap.end()) {
        return;
    }

    it.value().scaleFactor = 1.0;
    it.value().viewRect = QRectF(0, 0, it.value().image.cols, it.value().image.rows);
    
    Logger::instance()->info(QString("[RoiManager] 清除缩放状态: imageId=%1").arg(imageId));
}
