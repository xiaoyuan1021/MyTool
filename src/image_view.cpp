#include "image_view.h"
#include "qapplication.h"
#include <algorithm>   // 为 std::clamp

ImageView::ImageView(QWidget *parent)
    : QGraphicsView(parent),
    m_scene(new QGraphicsScene(this)),
    m_pixmapItem(new QGraphicsPixmapItem()),
    m_polygonMode(false),
    m_polygonPathItem(nullptr)
{
    setScene(m_scene);
    m_scene->addItem(m_pixmapItem);

    setRenderHints(QPainter::Antialiasing | QPainter::SmoothPixmapTransform);
    setDragMode(QGraphicsView::NoDrag);             // 后面可以再加拖动模式
    setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
}

void ImageView::setImage(const QImage &img)
{
    if(img.isNull()) return;

    m_pixmapItem->setPixmap(QPixmap::fromImage(img));
    m_pixmapItem->setPos(0, 0);

    //如何修改成自动铺满画布
    m_scene->setSceneRect(0, 0, img.width(), img.height());

    // 居中对齐
    setAlignment(Qt::AlignCenter);

    // 清理旧 ROI
    if (m_roiRectItem) {
        delete m_roiRectItem;
        m_roiRectItem = nullptr;
    }

    // 重置缩放
    resetTransform();
    m_scaleFactor = 1.0;

    if (this->width() > 0 && this->height() > 0) {
        fitInView(m_pixmapItem, Qt::KeepAspectRatio);
        QTransform t = transform();
        m_scaleFactor = t.m11();
    }
}

void ImageView::setRoiMode(bool enable)
{
    m_roiMode = enable;
    setDragMode(QGraphicsView::ScrollHandDrag);
}

void ImageView::clearRoi()
{
    if(m_roiRectItem)
    {
        delete m_roiRectItem;
        m_roiRectItem=nullptr;
    }
    m_isDrawingRoi=false;
}

void ImageView::finishRoiMode()
{
    m_roiMode=false;
    m_isDrawingRoi=false;
    setDragMode(QGraphicsView::NoDrag);
}

// =================== 缩放 ===================
void ImageView::wheelEvent(QWheelEvent *event)
{
    constexpr double scaleFactorStep = 1.25;
    if (event->angleDelta().y() > 0) {
        scale(scaleFactorStep, scaleFactorStep);
        m_scaleFactor *= scaleFactorStep;
    } else {
        scale(1.0 / scaleFactorStep, 1.0 / scaleFactorStep);
        m_scaleFactor /= scaleFactorStep;
    }
}

// =================== 坐标转换工具 ===================
QPointF ImageView::viewPosToImagePos(const QPoint &viewPos) const
{
    // 视图 → 场景
    QPointF scenePos = mapToScene(viewPos);

    // 场景 → 图像（pixmapItem 本地坐标）
    QPointF imgPos = m_pixmapItem->mapFromScene(scenePos);

    QSize imageSize = getImageSize();
    // 限制在图像范围内
    double x = std::clamp(imgPos.x(), 0.0, (double)imageSize.width());
    double y = std::clamp(imgPos.y(), 0.0, (double)imageSize.height());

    return QPointF(x, y);
}

// =================== 鼠标按下 ===================
void ImageView::mousePressEvent(QMouseEvent *event)
{
    if (m_polygonMode)
    {
        if (event->button() == Qt::LeftButton)
        {
            // 获取图像坐标
            QPointF imgPos = viewPosToImagePos(event->pos());

            // 添加到顶点列表
            m_polygonPoints.append(imgPos);

            // 更新显示
            updatePolygonPath(m_polygonPoints, m_polygonPathItem);

            // 发送信号（带类型标识）
            emit polygonDrawingPointAdded(m_currentDrawingType, imgPos);

            event->accept();
            return;
        }
        else if (event->button() == Qt::RightButton)
        {
            // 右键完成绘制
            if (m_polygonPoints.size() >= 3)  // 至少3个点才能成多边形
            {
                emit polygonDrawingFinished(m_currentDrawingType, m_polygonPoints);
                m_polygonMode = false;
            }

            event->accept();
            return;
        }
    }

    if(event->button()==Qt::RightButton && m_roiReady)
    {
        emit roiSelected(m_roiRectImg);
        m_roiReady=false;
        finishRoiMode();
        event->accept();
        return;
        qDebug()<<"ROI已裁剪";

    }
    if (m_roiMode && event->button() == Qt::LeftButton)
    {
        m_isDrawingRoi = true;

        // ⭐ 起点用“图像坐标”
        m_roiStartPosImg = viewPosToImagePos(event->pos());

        // 删旧 ROI
        if (m_roiRectItem) {
            delete m_roiRectItem;
            m_roiRectItem = nullptr;
        }

        // ⭐ ROI 作为 pixmapItem 的子项，坐标直接就是图像坐标
        m_roiRectItem = new QGraphicsRectItem(QRectF(m_roiStartPosImg, QSizeF(0, 0)), m_pixmapItem);
        //设置ROI颜色和线宽
        m_roiRectItem->setPen(QPen(Qt::green, 4, Qt::DashLine));

        setDragMode(QGraphicsView::NoDrag);
        event->accept();
        return;
    }
    setDragMode(QGraphicsView::ScrollHandDrag);
    QGraphicsView::mousePressEvent(event);
}

// =================== 鼠标移动 ===================
void ImageView::mouseMoveEvent(QMouseEvent *event)
{
    if (getImageSize().isEmpty()) {
        QGraphicsView::mouseMoveEvent(event);
        return;
    }

    // ROI 绘制中
    if (m_isDrawingRoi && m_roiRectItem)
    {
        QPointF curImgPos = viewPosToImagePos(event->pos());
        QRectF rect(m_roiStartPosImg, curImgPos);
        m_roiRectItem->setRect(rect.normalized());
        event->accept();
        return;
    }

    if(QApplication::keyboardModifiers() & Qt::ControlModifier)
    {
        // 像素信息：用图像坐标取像素
        QPointF imgPos = viewPosToImagePos(event->pos());
        int x = static_cast<int>(imgPos.x());
        int y = static_cast<int>(imgPos.y());

        QSize imageSize = getImageSize();

        if (x >= 0 && y >= 0 && x < imageSize.width() && y < imageSize.height())
        {
            QImage currentImg = m_pixmapItem->pixmap().toImage();
            QColor color = currentImg.pixelColor(x, y);
            int gray = qGray(color.rgb());
            emit pixelInfoChanged(x, y, color, gray);
        }
    }

    QGraphicsView::mouseMoveEvent(event);
}

// =================== 鼠标释放 ===================
void ImageView::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton && m_isDrawingRoi)
    {
        m_isDrawingRoi = false;

        if (m_roiRectItem)
        {
            // ⭐ rect 是 pixmapItem 本地坐标，即图像坐标
            m_roiRectImg = m_roiRectItem->rect();
            m_roiReady=(m_roiRectImg.width()>2 && m_roiRectImg.height()>2);
            //emit roiSelected(m_roiRectImg);
            //finishRoiMode();
            if(m_roiReady)
            {
                //qDebug()<<"[ROI] ready. Right click to crop:" << m_roiRectImg;
            }
        }
        event->accept();
        return;
    }

    QGraphicsView::mouseReleaseEvent(event);
}

void RoiManager::setFullImage(const cv::Mat &img)
{
    m_fullImage = img;
    m_isRoiActive = false;
    m_roiImage.release();

    // 📊 内存占用日志
    size_t bytes = img.total() * img.elemSize();
    // qDebug() << QString("[RoiManager] 图像已加载: %1x%2, 内存: %3 MB")
    //                 .arg(img.cols)
    //                 .arg(img.rows)
    //                 .arg(bytes / (1024.0 * 1024.0), 0, 'f', 2);
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

    // qDebug() << QString("[RoiManager] ROI已应用: x=%1 y=%2 w=%3 h=%4")
    //                 .arg(x).arg(y).arg(w).arg(h);
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


void ImageView::setPolygonMode(bool enable)
{
    if (enable) {
        startPolygonDrawing("region");  // ✅ 使用新函数
    } else {
        finishPolygonDrawing();
    }
}

void ImageView::clearPolygon()
{
    clearPolygonDrawing();
}

void ImageView::updatePolygonDisplay()
{
    updatePolygonPath(m_polygonPoints,m_polygonPathItem);
}

void ImageView::startPolygonDrawing(const QString &drawingType)
{
    m_currentDrawingType= drawingType;
    m_polygonMode=true;
    m_polygonPoints.clear();

    if(m_polygonPathItem)
    {
        delete m_polygonPathItem;
        m_polygonPathItem= nullptr;
    }
    Logger::instance()->info(QString("开始绘制%1 请点击左键添加顶点，右键完成")
                                 .arg(drawingType == "template" ? "模板" : "区域"));
}

void ImageView::finishPolygonDrawing()
{
    m_polygonMode=false;
    m_currentDrawingType.clear();
}

void ImageView::clearPolygonDrawing()
{
    m_polygonPoints.clear();
    m_polygonMode=false;
    m_currentDrawingType.clear();
    if(m_polygonPathItem)
    {
        delete m_polygonPathItem;
        m_polygonPathItem=nullptr;
    }
}

QVector<QPointF> ImageView::getPolygonPoints() const
{
    return m_polygonPoints;
}

void ImageView::updatePolygonPath(const QVector<QPointF> &points, QGraphicsPathItem *&pathItem)
{
    if(points.isEmpty()) return;
    if(pathItem)
    {
        delete pathItem;
        pathItem=nullptr;
    }
    QPainterPath path;
    path.moveTo(points.first());
    for(int i = 1;i < points.size(); ++i)
    {
        path.lineTo(points[i]);
    }
    QColor penColor =(m_currentDrawingType =="template") ? Qt::blue :Qt::red;
    pathItem = new QGraphicsPathItem(path, m_pixmapItem);
    pathItem->setPen(QPen(penColor, 2, Qt::SolidLine));
}
