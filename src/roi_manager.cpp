#include "roi_manager.h"
#include "logger.h"
#include "image_view.h"
#include <QStatusBar>
#include <QDebug>
#include <algorithm>

// ==================== 图像管理 ====================

void RoiManager::setFullImage(const cv::Mat &img)
{
    m_fullImage = img;
    // 单ROI模式兼容
    m_isRoiActive = false;
    m_roiImage.release();
}

const cv::Mat& RoiManager::getFullImage() const
{
    return m_fullImage;
}

void RoiManager::clear()
{
    m_fullImage.release();
    m_roiImage.release();
    m_isRoiActive = false;
    m_lastRoi = cv::Rect();
    // 多ROI模式
    m_rois.clear();
    m_selectedRoiId.clear();
    m_roiCounter = 0;
}

// ==================== 单ROI模式（向后兼容） ====================

const cv::Mat& RoiManager::getCurrentImage() const
{
    return m_isRoiActive ? m_roiImage : m_fullImage;
}

bool RoiManager::applyRoi(const QRectF &roiRectF)
{
    if (m_fullImage.empty()) {
        qDebug() << "[RoiManager] 完整图像为空";
        return false;
    }

    // 转换并验证ROI区域
    int x = std::max(0, (int)std::floor(roiRectF.x()));
    int y = std::max(0, (int)std::floor(roiRectF.y()));
    int w = std::min((int)std::ceil(roiRectF.width()),
                     m_fullImage.cols - x);
    int h = std::min((int)std::ceil(roiRectF.height()),
                     m_fullImage.rows - y);

    if (w <= 0 || h <= 0) {
        qDebug() << "[RoiManager] ROI区域无效";
        return false;
    }

    // 裁剪ROI
    cv::Rect roi(x, y, w, h);
    m_roiImage = m_fullImage(roi).clone();
    m_isRoiActive = true;
    m_lastRoi = roi;

    Logger::instance()->info(QString("[RoiManager] ROI已应用: x=%1 y=%2 w=%3 h=%4")
                                 .arg(x).arg(y).arg(w).arg(h));

    return true;
}

void RoiManager::resetRoi()
{
    // 无论ROI是否激活，都执行重置操作
    m_roiImage.release();
    m_isRoiActive = false;
    Logger::instance()->info("[RoiManager] ROI已重置");
}

bool RoiManager::isRoiActive() const
{
    return m_isRoiActive;
}

cv::Rect RoiManager::getLastRoi() const
{
    return m_lastRoi;
}

cv::Rect RoiManager::getRoiPosition() const
{
    return m_isRoiActive ? m_lastRoi : cv::Rect();
}

// ==================== 多ROI模式（新增） ====================

QString RoiManager::addRoi(const QString &name, const QRectF &rect)
{
    if (m_fullImage.empty()) {
        qDebug() << "[RoiManager] 完整图像为空，无法添加ROI";
        return QString();
    }

    RoiItem item(name, rect);
    if (!item.isValid()) {
        qDebug() << "[RoiManager] ROI区域无效";
        return QString();
    }

    // 确保ROI在图像范围内
    cv::Rect roiRect = item.rect();
    if (roiRect.x >= m_fullImage.cols || roiRect.y >= m_fullImage.rows) {
        qDebug() << "[RoiManager] ROI超出图像范围";
        return QString();
    }

    // 调整ROI大小以适应图像
    int w = std::min(roiRect.width, m_fullImage.cols - roiRect.x);
    int h = std::min(roiRect.height, m_fullImage.rows - roiRect.y);
    item.setRect(cv::Rect(roiRect.x, roiRect.y, w, h));

    QString roiId = item.id();
    m_rois.insert(roiId, item);

    Logger::instance()->info(QString("[RoiManager] 多ROI已添加: id=%1, name=%2, rect=(%3,%4,%5,%6)")
                                 .arg(roiId).arg(name)
                                 .arg(item.rect().x).arg(item.rect().y)
                                 .arg(item.rect().width).arg(item.rect().height));

    return roiId;
}

bool RoiManager::addRoiItem(const RoiItem &item)
{
    if (!item.isValid()) {
        qDebug() << "[RoiManager] RoiItem无效";
        return false;
    }

    QString roiId = item.id();
    if (m_rois.contains(roiId)) {
        qDebug() << "[RoiManager] ROI ID已存在:" << roiId;
        return false;
    }

    m_rois.insert(roiId, item);

    Logger::instance()->info(QString("[RoiManager] RoiItem已添加: %1").arg(item.toString()));

    return true;
}

bool RoiManager::removeRoi(const QString &roiId)
{
    if (!m_rois.contains(roiId)) {
        qDebug() << "[RoiManager] ROI不存在:" << roiId;
        return false;
    }

    m_rois.remove(roiId);

    // 如果删除的是选中的ROI，清除选中状态
    if (m_selectedRoiId == roiId) {
        m_selectedRoiId.clear();
    }

    Logger::instance()->info(QString("[RoiManager] ROI已移除: id=%1").arg(roiId));

    return true;
}

bool RoiManager::updateRoiRect(const QString &roiId, const QRectF &rect)
{
    if (!m_rois.contains(roiId)) {
        qDebug() << "[RoiManager] ROI不存在:" << roiId;
        return false;
    }

    RoiItem &item = m_rois[roiId];
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
    if (!m_rois.contains(roiId)) {
        qDebug() << "[RoiManager] ROI不存在:" << roiId;
        return false;
    }

    m_rois[roiId].setName(name);

    Logger::instance()->info(QString("[RoiManager] ROI名称已更新: id=%1, name=%2")
                                 .arg(roiId).arg(name));

    return true;
}

const RoiItem* RoiManager::getRoi(const QString &roiId) const
{
    auto it = m_rois.find(roiId);
    if (it == m_rois.end()) {
        return nullptr;
    }
    return &(*it);
}

RoiItem* RoiManager::getMutableRoi(const QString &roiId)
{
    auto it = m_rois.find(roiId);
    if (it == m_rois.end()) {
        return nullptr;
    }
    return &(*it);
}

QVector<RoiItem> RoiManager::getAllRois() const
{
    return m_rois.values().toVector();
}

QStringList RoiManager::getRoiIds() const
{
    return m_rois.keys();
}

int RoiManager::roiCount() const
{
    return m_rois.size();
}

bool RoiManager::hasRoi(const QString &roiId) const
{
    return m_rois.contains(roiId);
}

void RoiManager::clearAllRois()
{
    m_rois.clear();
    m_selectedRoiId.clear();
    m_roiCounter = 0;

    Logger::instance()->info("[RoiManager] 所有ROI已清空");
}

void RoiManager::setSelectedRoi(const QString &roiId)
{
    if (roiId.isEmpty() || m_rois.contains(roiId)) {
        m_selectedRoiId = roiId;

        if (!roiId.isEmpty()) {
            Logger::instance()->info(QString("[RoiManager] ROI已选中: id=%1").arg(roiId));
        }
    }
}

QString RoiManager::getSelectedRoiId() const
{
    return m_selectedRoiId;
}

cv::Mat RoiManager::cropRoi(const QString &roiId) const
{
    if (!m_rois.contains(roiId)) {
        qDebug() << "[RoiManager] ROI不存在:" << roiId;
        return cv::Mat();
    }

    const RoiItem &item = m_rois[roiId];
    return item.cropFromImage(m_fullImage);
}

QString RoiManager::generateDefaultName()
{
    m_roiCounter++;
    return QString("ROI_%1").arg(m_roiCounter, 3, 10, QChar('0'));
}

// ==================== UI交互相关方法 ====================

void RoiManager::enableDrawRoiMode(ImageView *view, QStatusBar *statusBar)
{
    if (m_fullImage.empty()) return;

    view->setRoiMode(true);
    view->setDragMode(QGraphicsView::NoDrag);
    if (statusBar) {
        statusBar->showMessage("请按下左键绘制ROI");
    }
}

bool RoiManager::handleRoiSelected(const QRectF &roiImgRectF, QStatusBar *statusBar)
{
    if (!applyRoi(roiImgRectF)) {
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
    QString name = roiName.isEmpty() ? generateDefaultName() : roiName;
    QString roiId = addRoi(name, roiImgRectF);

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
                .arg(name)
                .arg(roi.x).arg(roi.y).arg(roi.width).arg(roi.height),
            2000
        );
    }

    return roiId;
}

void RoiManager::resetRoiWithUI(ImageView *view, QStatusBar *statusBar)
{
    if (m_fullImage.empty()) return;

    resetRoi();
    view->clearRoi();

    if (statusBar) {
        statusBar->showMessage("ROI已重置，处理使用完整图像", 2000);
    }
    Logger::instance()->info("ROI已重置");
}