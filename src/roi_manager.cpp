#include "roi_manager.h"
#include "logger.h"
#include <QDebug>
#include <algorithm>

void RoiManager::setFullImage(const cv::Mat &img)
{
    m_fullImage = img;
    m_isRoiActive = false;
    m_roiImage.release();
}

const cv::Mat& RoiManager::getCurrentImage() const
{
    return m_isRoiActive ? m_roiImage : m_fullImage;
}

const cv::Mat& RoiManager::getFullImage() const
{
    return m_fullImage;
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
    if(m_isRoiActive)
    {
        m_roiImage.release();
        m_isRoiActive = false;
        Logger::instance()->info("[RoiManager] ROI已重置");
    }
}

bool RoiManager::isRoiActive() const
{
    return m_isRoiActive;
}

cv::Rect RoiManager::getLastRoi() const
{
    return m_lastRoi;
}

void RoiManager::clear()
{
    m_fullImage.release();
    m_roiImage.release();
    m_isRoiActive = false;
}
