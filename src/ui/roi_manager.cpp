#include "roi_manager.h"
#include "logger.h"
#include "image_view.h"
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
    if (img.empty()) return;

    if (m_currentImageId.isEmpty()) {
        QString imageId = addImage(img, generateDefaultImageName());
        switchToImage(imageId);
    } else {
        auto it = m_imageRoisMap.find(m_currentImageId);
        if (it != m_imageRoisMap.end()) {
            it.value().image = img.clone();
            it.value().activeRoiId.clear();  // 切换图片时清除激活状态
        }
    }
}

const cv::Mat& RoiManager::getFullImage() const
{
    auto it = m_imageRoisMap.find(m_currentImageId);
    if (it != m_imageRoisMap.end()) {
        return it.value().image;
    }
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
    return addImage(img, name, QString());
}

QString RoiManager::addImage(const cv::Mat &img, const QString &name, const QString &filePath)
{
    if (img.empty()) {
        qDebug() << "[RoiManager] 图像为空，无法添加";
        return QString();
    }

    QString imageId = QString("img_%1_%2")
        .arg(QDateTime::currentMSecsSinceEpoch())
        .arg(m_imageCounter++);

    ImageRois imageRois;
    imageRois.image = img.clone();
    imageRois.name = name.isEmpty() ? generateDefaultImageName() : name;
    imageRois.filePath = filePath;
    imageRois.roiCounter = 0;
    imageRois.activeRoiId.clear();
    imageRois.scaleFactor = 1.0;
    imageRois.viewRect = QRectF(0, 0, img.cols, img.rows);

    m_imageRoisMap.insert(imageId, imageRois);

    Logger::instance()->info(QString("[RoiManager] 图片已添加: id=%1, name=%2, filePath=%3")
                                 .arg(imageId).arg(imageRois.name).arg(filePath));

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

QString RoiManager::getImageFilePath(const QString &imageId) const
{
    auto it = m_imageRoisMap.find(imageId);
    if (it != m_imageRoisMap.end()) {
        return it.value().filePath;
    }
    return QString();
}

int RoiManager::imageCount() const
{
    return m_imageRoisMap.size();
}

// ==================== 单ROI模式（基于 RoiConfig）====================

cv::Mat RoiManager::getCurrentImage() const
{
    auto it = m_imageRoisMap.find(m_currentImageId);
    if (it == m_imageRoisMap.end()) {
        return cv::Mat();
    }

    const ImageRois& imageRois = it.value();
    if (imageRois.image.empty()) {
        return cv::Mat();
    }

    // 没有激活的ROI，返回完整图像
    if (imageRois.activeRoiId.isEmpty()) {
        return imageRois.image;
    }

    // 查找激活的ROI配置并裁剪
    for (const auto& cfg : imageRois.roiConfigs) {
        if (cfg.roiId == imageRois.activeRoiId) {
            int x = std::max(0, (int)std::floor(cfg.roiRect.x()));
            int y = std::max(0, (int)std::floor(cfg.roiRect.y()));
            int w = std::min((int)std::ceil(cfg.roiRect.width()), imageRois.image.cols - x);
            int h = std::min((int)std::ceil(cfg.roiRect.height()), imageRois.image.rows - y);

            if (w > 0 && h > 0) {
                return imageRois.image(cv::Rect(x, y, w, h)).clone();
            }
            break;
        }
    }

    // 激活的ROI无效，返回完整图像
    return imageRois.image;
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
    int w = std::min((int)std::ceil(roiRectF.width()), imageRois.image.cols - x);
    int h = std::min((int)std::ceil(roiRectF.height()), imageRois.image.rows - y);

    if (w <= 0 || h <= 0) {
        qDebug() << "[RoiManager] ROI区域无效";
        return false;
    }

    QRectF normalizedRect(x, y, w, h);

    // 在现有roiConfigs中查找匹配的ROI（通过矩形匹配，带容差）
    for (auto& cfg : imageRois.roiConfigs) {
        double tolerance = 2.0; // 2像素容差
        if (qAbs(cfg.roiRect.x() - normalizedRect.x()) <= tolerance &&
            qAbs(cfg.roiRect.y() - normalizedRect.y()) <= tolerance &&
            qAbs(cfg.roiRect.width() - normalizedRect.width()) <= tolerance &&
            qAbs(cfg.roiRect.height() - normalizedRect.height()) <= tolerance) {
            imageRois.activeRoiId = cfg.roiId;
            Logger::instance()->info(QString("[RoiManager] ROI已激活(矩形匹配): %1 (原始矩形与存储矩形差异在容差范围内)").arg(cfg.roiName));
            return true;
        }
    }

    // 未找到匹配的ROI，创建新的RoiConfig
    QString roiName = generateDefaultRoiName();
    RoiConfig newCfg(roiName, normalizedRect);
    imageRois.roiConfigs.append(newCfg);
    imageRois.activeRoiId = newCfg.roiId;

    Logger::instance()->info(QString("[RoiManager] ROI已创建并激活: %1 (x=%2 y=%3 w=%4 h=%5)")
                                 .arg(roiName).arg(x).arg(y).arg(w).arg(h));

    emit roiConfigChanged();
    return true;
}

void RoiManager::resetRoi()
{
    auto it = m_imageRoisMap.find(m_currentImageId);
    if (it == m_imageRoisMap.end()) {
        return;
    }

    it.value().activeRoiId.clear();
    Logger::instance()->info("[RoiManager] ROI已重置");
}

bool RoiManager::isRoiActive() const
{
    auto it = m_imageRoisMap.find(m_currentImageId);
    if (it == m_imageRoisMap.end()) {
        return false;
    }
    return !it.value().activeRoiId.isEmpty();
}

cv::Rect RoiManager::getLastRoi() const
{
    auto it = m_imageRoisMap.find(m_currentImageId);
    if (it == m_imageRoisMap.end()) {
        return cv::Rect();
    }

    const ImageRois& imageRois = it.value();
    if (imageRois.activeRoiId.isEmpty()) {
        return cv::Rect();
    }

    for (const auto& cfg : imageRois.roiConfigs) {
        if (cfg.roiId == imageRois.activeRoiId) {
            int x = std::max(0, (int)std::floor(cfg.roiRect.x()));
            int y = std::max(0, (int)std::floor(cfg.roiRect.y()));
            int w = std::min((int)std::ceil(cfg.roiRect.width()), imageRois.image.cols - x);
            int h = std::min((int)std::ceil(cfg.roiRect.height()), imageRois.image.rows - y);
            if (w > 0 && h > 0) {
                return cv::Rect(x, y, w, h);
            }
            break;
        }
    }

    return cv::Rect();
}

cv::Rect RoiManager::getRoiPosition() const
{
    return getLastRoi();
}

// ==================== ROI配置管理（统一数据源）====================

void RoiManager::addRoiConfig(const RoiConfig& config)
{
    auto it = m_imageRoisMap.find(m_currentImageId);
    if (it == m_imageRoisMap.end()) {
        qDebug() << "[RoiManager] 没有当前图片，无法添加ROI配置";
        return;
    }

    it.value().roiConfigs.append(config);
    Logger::instance()->info(QString("[RoiManager] ROI配置已添加: %1 (id=%2, rect=%3,%4,%5,%6)")
        .arg(config.roiName).arg(config.roiId)
        .arg(config.roiRect.x(), 0, 'f', 1)
        .arg(config.roiRect.y(), 0, 'f', 1)
        .arg(config.roiRect.width(), 0, 'f', 1)
        .arg(config.roiRect.height(), 0, 'f', 1));
    emit roiConfigChanged();
}

bool RoiManager::removeRoiConfig(const QString& roiId)
{
    auto it = m_imageRoisMap.find(m_currentImageId);
    if (it == m_imageRoisMap.end()) {
        return false;
    }

    ImageRois& imageRois = it.value();
    for (int i = 0; i < imageRois.roiConfigs.size(); ++i) {
        if (imageRois.roiConfigs[i].roiId == roiId) {
            Logger::instance()->info(QString("[RoiManager] ROI配置已移除: %1")
                                         .arg(imageRois.roiConfigs[i].roiName));
            imageRois.roiConfigs.removeAt(i);

            // 如果删除的是当前激活的ROI，清除激活状态
            if (imageRois.activeRoiId == roiId) {
                imageRois.activeRoiId.clear();
            }

            emit roiConfigChanged();
            return true;
        }
    }
    return false;
}

RoiConfig* RoiManager::getRoiConfig(const QString& roiId)
{
    auto it = m_imageRoisMap.find(m_currentImageId);
    if (it == m_imageRoisMap.end()) {
        return nullptr;
    }

    for (auto& config : it.value().roiConfigs) {
        if (config.roiId == roiId) {
            return &config;
        }
    }
    return nullptr;
}

const RoiConfig* RoiManager::getRoiConfig(const QString& roiId) const
{
    auto it = m_imageRoisMap.find(m_currentImageId);
    if (it == m_imageRoisMap.end()) {
        return nullptr;
    }

    for (const auto& config : it.value().roiConfigs) {
        if (config.roiId == roiId) {
            return &config;
        }
    }
    return nullptr;
}

QList<RoiConfig>& RoiManager::getRoiConfigs()
{
    static QList<RoiConfig> emptyList;
    auto it = m_imageRoisMap.find(m_currentImageId);
    if (it == m_imageRoisMap.end()) {
        return emptyList;
    }
    return it.value().roiConfigs;
}

const QList<RoiConfig>& RoiManager::getRoiConfigs() const
{
    static const QList<RoiConfig> emptyList;
    auto it = m_imageRoisMap.find(m_currentImageId);
    if (it == m_imageRoisMap.end()) {
        return emptyList;
    }
    return it.value().roiConfigs;
}

QList<RoiConfig> RoiManager::getRoiConfigsForImage(const QString& imageId) const
{
    auto it = m_imageRoisMap.find(imageId);
    if (it == m_imageRoisMap.end()) {
        return {};
    }
    return it.value().roiConfigs;
}

int RoiManager::getRoiConfigCount() const
{
    auto it = m_imageRoisMap.find(m_currentImageId);
    if (it == m_imageRoisMap.end()) {
        return 0;
    }
    return it.value().roiConfigs.size();
}

void RoiManager::clearRoiConfigs()
{
    auto it = m_imageRoisMap.find(m_currentImageId);
    if (it == m_imageRoisMap.end()) {
        return;
    }
    it.value().roiConfigs.clear();
    it.value().activeRoiId.clear();
    emit roiConfigChanged();
}

// ==================== 激活ROI管理 ====================

void RoiManager::setActiveRoi(const QString& roiId)
{
    auto it = m_imageRoisMap.find(m_currentImageId);
    if (it == m_imageRoisMap.end()) {
        return;
    }

    ImageRois& imageRois = it.value();

    if (roiId.isEmpty()) {
        imageRois.activeRoiId.clear();
        return;
    }

    for (const auto& cfg : imageRois.roiConfigs) {
        if (cfg.roiId == roiId) {
            imageRois.activeRoiId = roiId;
            Logger::instance()->info(QString("[RoiManager] ROI已激活: id=%1").arg(roiId));
            return;
        }
    }

    qDebug() << "[RoiManager] ROI不存在:" << roiId;
}

QString RoiManager::getActiveRoiId() const
{
    auto it = m_imageRoisMap.find(m_currentImageId);
    if (it == m_imageRoisMap.end()) {
        return QString();
    }
    return it.value().activeRoiId;
}

void RoiManager::clearActiveRoi()
{
    auto it = m_imageRoisMap.find(m_currentImageId);
    if (it == m_imageRoisMap.end()) {
        return;
    }
    it.value().activeRoiId.clear();
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

// ==================== 全量配置导出/导入 ====================

QJsonDocument RoiManager::exportAllConfigsToJson() const
{
    QJsonObject root;
    root["version"] = "1.0";
    root["currentImageId"] = m_currentImageId;

    QJsonObject imagesObj;
    for (auto it = m_imageRoisMap.constBegin(); it != m_imageRoisMap.constEnd(); ++it) {
        QJsonObject imageObj;
        imageObj["name"] = it.value().name;
        imageObj["filePath"] = it.value().filePath;

        QJsonArray configsArray;
        for (const auto& config : it.value().roiConfigs) {
            configsArray.append(config.toJson());
        }
        imageObj["roiConfigs"] = configsArray;
        imageObj["activeRoiId"] = it.value().activeRoiId;

        imagesObj[it.key()] = imageObj;
    }
    root["images"] = imagesObj;

    return QJsonDocument(root);
}

void RoiManager::importAllConfigsFromJson(const QJsonDocument& doc)
{
    QJsonObject root = doc.object();

    if (root.contains("images")) {
        QJsonObject imagesObj = root["images"].toObject();
        for (auto it = imagesObj.begin(); it != imagesObj.end(); ++it) {
            QString imageId = it.key();
            QJsonObject imageObj = it.value().toObject();

            if (m_imageRoisMap.contains(imageId)) {
                ImageRois& imageRois = m_imageRoisMap[imageId];
                imageRois.roiConfigs.clear();

                QJsonArray configsArray = imageObj["roiConfigs"].toArray();
                for (const auto& val : configsArray) {
                    RoiConfig config;
                    config.fromJson(val.toObject());
                    imageRois.roiConfigs.append(config);
                }
                imageRois.activeRoiId = imageObj["activeRoiId"].toString();

                Logger::instance()->info(QString("[RoiManager] 已恢复图片ROI配置: imageId=%1, count=%2")
                                             .arg(imageId).arg(imageRois.roiConfigs.size()));
            }
        }
    }

    if (root.contains("currentImageId")) {
        QString savedImageId = root["currentImageId"].toString();
        if (m_imageRoisMap.contains(savedImageId)) {
            switchToImage(savedImageId);
        }
    }

    emit roiConfigChanged();
}

// ==================== 命名生成 ====================

QString RoiManager::generateNextRoiName() const
{
    auto it = m_imageRoisMap.find(m_currentImageId);
    if (it == m_imageRoisMap.end() || it.value().roiConfigs.isEmpty()) {
        return QString("ROI_1");
    }

    int maxNum = 0;
    for (const auto& cfg : it.value().roiConfigs) {
        QString name = cfg.roiName;
        if (name.startsWith("ROI_")) {
            QString numStr = name.mid(4);
            bool ok = false;
            int num = numStr.toInt(&ok);
            if (ok && num > maxNum) {
                maxNum = num;
            }
        }
    }
    return QString("ROI_%1").arg(maxNum + 1);
}

QString RoiManager::generateDefaultRoiName()
{
    // 【Bug修复】统一使用generateNextRoiName，避免删除后编号重复
    return generateNextRoiName();
}

QString RoiManager::generateDefaultImageName()
{
    return QString("图片_%1").arg(m_imageCounter + 1);
}
