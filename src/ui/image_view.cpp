#include "image_view.h"
#include "logger.h"
#include "qapplication.h"
#include <QAction>
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

/// 图像设置内部实现
/// @param keepZoom true=保持当前缩放比例, false=重置缩放并自适应窗口
void ImageView::setImageInternal(const QImage &img, bool keepZoom)
{
    if (img.isNull()) return;

    // 设置图像内容
    m_pixmapItem->setPixmap(QPixmap::fromImage(img));
    m_pixmapItem->setPos(0, 0);
    m_scene->setSceneRect(0, 0, img.width(), img.height());
    setAlignment(Qt::AlignCenter);

    // 清理旧 ROI
    if (m_roiRectItem) {
        delete m_roiRectItem;
        m_roiRectItem = nullptr;
    }
    m_roiState = RoiState::None;
    m_roiHandle = None;

    // 根据参数决定是否重置缩放
    if (!keepZoom) {
        resetTransform();
        m_scaleFactor = 1.0;
        // 自适应窗口大小
        if (this->width() > 0 && this->height() > 0) {
            fitInView(m_pixmapItem, Qt::KeepAspectRatio);
            m_scaleFactor = transform().m11();
        }
    }
}

/// 设置图像并重置缩放（自适应窗口）
void ImageView::setImage(const QImage &img)
{
    setImageInternal(img, false);
}

/// 设置图像并保持当前缩放比例
void ImageView::setImageKeepZoom(const QImage &img)
{
    setImageInternal(img, true);
}

void ImageView::setRoiMode(bool enable)
{
    m_roiState = enable ? RoiState::Drawing : RoiState::None;
    setDragMode(enable ? QGraphicsView::ScrollHandDrag : QGraphicsView::NoDrag);
}

void ImageView::clearRoi()
{
    if(m_roiRectItem)
    {
        delete m_roiRectItem;
        m_roiRectItem=nullptr;
    }
    m_roiState = RoiState::None;
    m_roiHandle=None;
}

void ImageView::finishRoiMode()
{
    clearRoi();
    setDragMode(QGraphicsView::NoDrag);
}

// =================== 缩放 ===================
void ImageView::wheelEvent(QWheelEvent *event)
{
    if (!m_zoomEnabled) {
        event->accept();
        return;
    }

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
    // 如果处于缩放禁用状态，不处理任何鼠标事件
    if (!m_zoomEnabled) {
        QGraphicsView::mousePressEvent(event);
        return;
    }

    // ROI编辑/就绪模式 - 调整大小（仅左键触发）
    if ((m_roiState == RoiState::Editing || m_roiState == RoiState::Ready) && event->button() == Qt::LeftButton)
    {
        RoiHandle handle = getRoiHandleAtPos(viewPosToImagePos(event->pos()));
        if (handle != None)
        {
            m_roiHandle = handle;
            m_dragStartPos = viewPosToImagePos(event->pos());
            m_dragStartRect = m_roiRectItem->rect();
            setDragMode(QGraphicsView::NoDrag);
            event->accept();
            return;
        }
    }

    // 多边形绘制模式
    if (m_polygonMode) {
        handlePolygonPressEvent(event);
        return;
    }

    // 矩形绘制模式
    if (m_rectangleMode) {
        handleRectanglePressEvent(event);
        return;
    }

    // 参考线绘制模式
    if (m_referenceLineMode) {
        handleReferenceLinePressEvent(event);
        return;
    }

    // 右键上下文菜单（不在任何绘制模式下）
    if (event->button() == Qt::RightButton
        && !m_polygonMode && !m_rectangleMode && !m_referenceLineMode 
        && m_roiState != RoiState::Drawing && m_roiState != RoiState::Ready)
    {
        showContextMenu(event->pos());
        event->accept();
        return;
    }

    // ROI选择确认
    if (event->button() == Qt::RightButton && m_roiState == RoiState::Ready) {
        handleRoiConfirmPressEvent(event);
        return;
    }

    // ROI绘制模式
    if (m_roiState == RoiState::Drawing && event->button() == Qt::LeftButton) {
        handleRoiDrawStartPressEvent(event);
        return;
    }

    setDragMode(QGraphicsView::ScrollHandDrag);
    QGraphicsView::mousePressEvent(event);
}

// =================== 多边形绘制处理 ===================
void ImageView::handlePolygonPressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        QPointF imgPos = viewPosToImagePos(event->pos());
        m_polygonPoints.append(imgPos);
        updatePolygonPath(m_polygonPoints, m_polygonPathItem);
        emit polygonDrawingPointAdded(m_currentDrawingType, imgPos);
        event->accept();
    } else if (event->button() == Qt::RightButton) {
        if (m_polygonPoints.size() >= 3) {
            emit polygonDrawingFinished(m_currentDrawingType, m_polygonPoints);
            m_polygonMode = false;
        }
        event->accept();
    }
}

// =================== 矩形绘制处理 ===================
void ImageView::handleRectanglePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        m_rectStartPosImg = viewPosToImagePos(event->pos());

        if (m_rectItem) {
            delete m_rectItem;
            m_rectItem = nullptr;
        }

        m_rectItem = new QGraphicsRectItem(QRectF(m_rectStartPosImg, QSizeF(0, 0)), m_pixmapItem);
        QSize imgSize = getImageSize();
        double imageScale = std::max(imgSize.width(), imgSize.height()) / 5000.0;
        double adaptiveWidth = std::max(2.0, imageScale * 3.0);

        QColor rectColor = (m_currentRectDrawingType == "template") ? Qt::blue : Qt::red;
        m_rectItem->setPen(QPen(rectColor, adaptiveWidth, Qt::SolidLine));

        setDragMode(QGraphicsView::NoDrag);
        event->accept();
    }
}

// =================== 参考线绘制处理 ===================
void ImageView::handleReferenceLinePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        QPointF imgPos = viewPosToImagePos(event->pos());
        
        if (m_refLineStartPosImg.isNull()) {
            // 设置起点
            m_refLineStartPosImg = imgPos;
            m_referenceLineWaitingConfirm = false;
            
            // 显示起点标记
            showReferenceLineHint("已设置起点，请继续点击设置终点");
            
            Logger::instance()->info(QString("参考线起点已设置: (%1, %2)，请点击终点")
                .arg(imgPos.x(), 0, 'f', 1)
                .arg(imgPos.y(), 0, 'f', 1));
        } else if (!m_referenceLineWaitingConfirm) {
            // 设置终点，显示预览，等待右键确认
            if (m_refLinePreviewItem) {
                delete m_refLinePreviewItem;
                m_refLinePreviewItem = nullptr;
            }
            
            QSize imgSize = getImageSize();
            double imageScale = std::max(imgSize.width(), imgSize.height()) / 5000.0;
            double adaptiveWidth = std::max(3.0, imageScale * 4.0);
            
            m_refLinePreviewItem = new QGraphicsLineItem(
                QLineF(m_refLineStartPosImg, imgPos), m_pixmapItem);
            m_refLinePreviewItem->setPen(QPen(QColor(255, 255, 0), adaptiveWidth, Qt::DashLine));
            
            m_referenceLineWaitingConfirm = true;
            showReferenceLineHint("预览线已显示，右键确认绘制 / 左键重新设置终点");
            
            Logger::instance()->info(QString("参考线终点已设置: (%1, %2)，右键确认")
                .arg(imgPos.x(), 0, 'f', 1)
                .arg(imgPos.y(), 0, 'f', 1));
        } else {
            // 已在等待确认状态，左键点击重新设置终点
            if (m_refLinePreviewItem) {
                delete m_refLinePreviewItem;
                m_refLinePreviewItem = nullptr;
            }
            
            QSize imgSize = getImageSize();
            double imageScale = std::max(imgSize.width(), imgSize.height()) / 5000.0;
            double adaptiveWidth = std::max(3.0, imageScale * 4.0);
            
            m_refLinePreviewItem = new QGraphicsLineItem(
                QLineF(m_refLineStartPosImg, imgPos), m_pixmapItem);
            m_refLinePreviewItem->setPen(QPen(QColor(255, 255, 0), adaptiveWidth, Qt::DashLine));
            
            showReferenceLineHint("预览线已更新，右键确认绘制");
        }
        
        event->accept();
    } else if (event->button() == Qt::RightButton) {
        if (m_referenceLineWaitingConfirm && m_refLinePreviewItem) {
            // 右键确认：将预览线转为正式参考线
            QLineF line = m_refLinePreviewItem->line();
            cv::Point2f start(line.p1().x(), line.p1().y());
            cv::Point2f end(line.p2().x(), line.p2().y());
            
            // 创建正式参考线
            if (m_referenceLineItem) {
                delete m_referenceLineItem;
                m_referenceLineItem = nullptr;
            }
            
            QSize imgSize = getImageSize();
            double imageScale = std::max(imgSize.width(), imgSize.height()) / 5000.0;
            double adaptiveWidth = std::max(3.0, imageScale * 4.0);
            
            m_referenceLineItem = new QGraphicsLineItem(
                QLineF(m_refLineStartPosImg, QPointF(end.x, end.y)), m_pixmapItem);
            m_referenceLineItem->setPen(QPen(QColor(0, 255, 0), adaptiveWidth, Qt::SolidLine));
            
            // 删除预览线
            delete m_refLinePreviewItem;
            m_refLinePreviewItem = nullptr;
            
            emit referenceLineDrawn(start, end);
            
            Logger::instance()->info(QString("参考线绘制确认: 起点(%1,%2) -> 终点(%3,%4)")
                .arg(start.x, 0, 'f', 1).arg(start.y, 0, 'f', 1)
                .arg(end.x, 0, 'f', 1).arg(end.y, 0, 'f', 1));
            
            showReferenceLineHint("参考线已确认，可继续操作");
            m_referenceLineMode = false;
            m_referenceLineWaitingConfirm = false;
        } else {
            // 右键取消（未设置终点时）
            clearReferenceLine();
            Logger::instance()->info("参考线绘制已取消");
        }
        
        event->accept();
    }
}

// =================== ROI确认处理 ===================
void ImageView::handleRoiConfirmPressEvent(QMouseEvent *event)
{
    emit roiSelected(m_roiRectImg);
    finishRoiMode();
    event->accept();
}

// =================== ROI绘制开始处理 ===================
void ImageView::handleRoiDrawStartPressEvent(QMouseEvent *event)
{
    m_roiState = RoiState::Drawing;
    m_roiStartPosImg = viewPosToImagePos(event->pos());

    if (m_roiRectItem) {
        delete m_roiRectItem;
        m_roiRectItem = nullptr;
    }

    m_roiRectItem = new QGraphicsRectItem(QRectF(m_roiStartPosImg, QSizeF(0, 0)), m_pixmapItem);

    QSize imgSize = getImageSize();
    double imageScale = std::max(imgSize.width(), imgSize.height()) / 5000.0;
    double adaptiveWidth = std::max(2.0, imageScale * 3.0);

    m_roiRectItem->setPen(QPen(Qt::green, adaptiveWidth, Qt::SolidLine));

    setDragMode(QGraphicsView::NoDrag);
    event->accept();
}

// =================== 鼠标移动 ===================
void ImageView::mouseMoveEvent(QMouseEvent *event)
{
    // ROI拖拽或调整大小中
    if (m_roiHandle != None && m_roiRectItem) {
        QPointF curImgPos = viewPosToImagePos(event->pos());
        QPointF delta = curImgPos - m_dragStartPos;
        QRectF newRect = m_dragStartRect;
        
        // 根据把手类型更新矩形
        switch (m_roiHandle) {
            case Move: {
                // 移动整个ROI
                newRect.translate(delta);
                // 限制在图像范围内
                QSize imgSize = getImageSize();
                if (newRect.left() < 0) {
                    newRect.moveLeft(0);
                }
                if (newRect.top() < 0) {
                    newRect.moveTop(0);
                }
                if (newRect.right() > imgSize.width()) {
                    newRect.moveRight(imgSize.width());
                }
                if (newRect.bottom() > imgSize.height()) {
                    newRect.moveBottom(imgSize.height());
                }
                break;
            }
            case TopLeft:
                newRect.setTopLeft(m_dragStartRect.topLeft() + delta);
                break;
            case TopRight:
                newRect.setTopRight(m_dragStartRect.topRight() + delta);
                break;
            case BottomLeft:
                newRect.setBottomLeft(m_dragStartRect.bottomLeft() + delta);
                break;
            case BottomRight:
                newRect.setBottomRight(m_dragStartRect.bottomRight() + delta);
                break;
            case Top:
                newRect.setTop(m_dragStartRect.top() + delta.y());
                break;
            case Bottom:
                newRect.setBottom(m_dragStartRect.bottom() + delta.y());
                break;
            case Left:
                newRect.setLeft(m_dragStartRect.left() + delta.x());
                break;
            case Right:
                newRect.setRight(m_dragStartRect.right() + delta.x());
                break;
            default:
                break;
        }
        
        // 确保矩形不会反转（宽度和高度至少为2像素）
        if (newRect.width() < 2) {
            newRect.setWidth(2);
        }
        if (newRect.height() < 2) {
            newRect.setHeight(2);
        }
        
        m_roiRectItem->setRect(newRect.normalized());
        m_roiRectImg = newRect.normalized();
        event->accept();
        return;
    }

    // 当没有拖拽且不在任何绘制模式时，检测鼠标位置以更新光标
    if ((m_roiState == RoiState::Editing || m_roiState == RoiState::Ready) && !m_polygonMode && !m_rectangleMode && !m_referenceLineMode) {
        QPointF imgPos = viewPosToImagePos(event->pos());
        RoiHandle handle = getRoiHandleAtPos(imgPos);
        setCursorForHandle(handle);
    }

    if (getImageSize().isEmpty()) {
        QGraphicsView::mouseMoveEvent(event);
        return;
    }

    // ROI 绘制中
    if (m_roiState == RoiState::Drawing && m_roiRectItem)
    {
        QPointF curImgPos = viewPosToImagePos(event->pos());
        QRectF rect(m_roiStartPosImg, curImgPos);
        m_roiRectItem->setRect(rect.normalized());
        event->accept();
        return;
    }

    // 矩形绘制中
    if (m_rectangleMode && m_rectItem)
    {
        QPointF curImgPos = viewPosToImagePos(event->pos());
        QRectF rect(m_rectStartPosImg, curImgPos);
        m_rectItem->setRect(rect.normalized());
        event->accept();
        return;
    }

    // 参考线绘制中：显示预览线
    if (m_referenceLineMode && !m_refLineStartPosImg.isNull() && !m_referenceLineWaitingConfirm)
    {
        QPointF curImgPos = viewPosToImagePos(event->pos());
        
        if (m_refLinePreviewItem) {
            m_refLinePreviewItem->setLine(QLineF(m_refLineStartPosImg, curImgPos));
        } else {
            QSize imgSize = getImageSize();
            double imageScale = std::max(imgSize.width(), imgSize.height()) / 5000.0;
            double adaptiveWidth = std::max(3.0, imageScale * 4.0);
            
            m_refLinePreviewItem = new QGraphicsLineItem(
                QLineF(m_refLineStartPosImg, curImgPos), m_pixmapItem);
            m_refLinePreviewItem->setPen(QPen(QColor(255, 255, 0, 128), adaptiveWidth, Qt::DashLine));
        }
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
    // 结束ROI拖拽或调整
    if (m_roiHandle != None) {
        m_roiHandle = None;
        
        // 更新ROI矩形数据
        if (m_roiRectItem) {
            m_roiRectImg = m_roiRectItem->rect();
            
            // 检查ROI是否仍然有效
            if (m_roiRectImg.width() <= 2 || m_roiRectImg.height() <= 2) {
                m_roiState = RoiState::None;
            }
        }
        
        // 恢复光标
        if ((m_roiState == RoiState::Editing || m_roiState == RoiState::Ready) && m_roiRectItem) {
            QPointF imgPos = viewPosToImagePos(event->pos());
            RoiHandle handle = getRoiHandleAtPos(imgPos);
            setCursorForHandle(handle);
        } else {
            setCursor(Qt::ArrowCursor);
        }
        
        event->accept();
        return;
    }

    if (event->button() == Qt::LeftButton && m_roiState == RoiState::Drawing)
    {
        if (m_roiRectItem)
        {
            // rect 是 pixmapItem 本地坐标，即图像坐标
            m_roiRectImg = m_roiRectItem->rect();
            // ROI尺寸足够大时进入Ready状态，否则回到None
            if (m_roiRectImg.width() > 2 && m_roiRectImg.height() > 2) {
                m_roiState = RoiState::Ready;
            } else {
                m_roiState = RoiState::None;
            }
        }
        event->accept();
        return;
    }

    // 矩形绘制完成（松开左键直接确认）
    if (event->button() == Qt::LeftButton && m_rectangleMode && m_rectItem)
    {
        QRectF rectImg = m_rectItem->rect();
        if (rectImg.width() > 2 && rectImg.height() > 2) {
            QVector<QPointF> rectPoints;
            rectPoints.append(rectImg.topLeft());
            rectPoints.append(rectImg.topRight());
            rectPoints.append(rectImg.bottomRight());
            rectPoints.append(rectImg.bottomLeft());
            emit polygonDrawingFinished(m_currentRectDrawingType, rectPoints);
        }
        clearRectangleDrawing();
        event->accept();
        return;
    }

    QGraphicsView::mouseReleaseEvent(event);
}

// 检测鼠标在ROI框上的位置
RoiHandle ImageView::getRoiHandleAtPos(const QPointF &imgPos) const
{
    if (!m_roiRectItem || m_roiState == RoiState::None || m_roiState == RoiState::Drawing) {
        return None;
    }

    // 获取当前ROI矩形（图像坐标）
    QRectF rect = m_roiRectItem->rect();
    
    // 计算把手检测范围（考虑缩放比例）
    double handleScale = 1.0 / m_scaleFactor;
    double handleSize = m_handleSize * handleScale;

    // 扩展检测范围，便于用户点击
    double tolerance = handleSize * 2.0;

    // 判断是否在四个角
    if (imgPos.x() >= rect.left() - tolerance && imgPos.x() <= rect.left() + tolerance &&
        imgPos.y() >= rect.top() - tolerance && imgPos.y() <= rect.top() + tolerance) {
        return TopLeft;
    }
    if (imgPos.x() >= rect.right() - tolerance && imgPos.x() <= rect.right() + tolerance &&
        imgPos.y() >= rect.top() - tolerance && imgPos.y() <= rect.top() + tolerance) {
        return TopRight;
    }
    if (imgPos.x() >= rect.left() - tolerance && imgPos.x() <= rect.left() + tolerance &&
        imgPos.y() >= rect.bottom() - tolerance && imgPos.y() <= rect.bottom() + tolerance) {
        return BottomLeft;
    }
    if (imgPos.x() >= rect.right() - tolerance && imgPos.x() <= rect.right() + tolerance &&
        imgPos.y() >= rect.bottom() - tolerance && imgPos.y() <= rect.bottom() + tolerance) {
        return BottomRight;
    }

    // 判断是否在四条边（但不包括角）
    if (imgPos.x() >= rect.left() && imgPos.x() <= rect.right()) {
        if (imgPos.y() >= rect.top() - tolerance && imgPos.y() <= rect.top() + tolerance) {
            return Top;
        }
        if (imgPos.y() >= rect.bottom() - tolerance && imgPos.y() <= rect.bottom() + tolerance) {
            return Bottom;
        }
    }
    
    if (imgPos.y() >= rect.top() && imgPos.y() <= rect.bottom()) {
        if (imgPos.x() >= rect.left() - tolerance && imgPos.x() <= rect.left() + tolerance) {
            return Left;
        }
        if (imgPos.x() >= rect.right() - tolerance && imgPos.x() <= rect.right() + tolerance) {
            return Right;
        }
    }

    // 判断是否在ROI内部（可以拖拽整个ROI）
    if (rect.contains(imgPos)) {
        return Move;
    }

    return None;
}

void ImageView::setCursorForHandle(RoiHandle handle)
{
   Qt::CursorShape cursor;
    
    switch (handle) {
        case TopLeft:
        case BottomRight:
            cursor = Qt::SizeFDiagCursor;
            break;
        case TopRight:
        case BottomLeft:
            cursor = Qt::SizeBDiagCursor;
            break;
        case Top:
        case Bottom:
            cursor = Qt::SizeVerCursor;
            break;
        case Left:
        case Right:
            cursor = Qt::SizeHorCursor;
            break;
        case Move:
            cursor = Qt::OpenHandCursor;
            break;
        default:
            cursor = Qt::ArrowCursor;
            break;
    }

    setCursor(cursor);
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

void ImageView::startRectangleDrawing(const QString &drawingType)
{
    m_rectangleMode = true;
    m_currentRectDrawingType = drawingType;

    if (m_rectItem) {
        delete m_rectItem;
        m_rectItem = nullptr;
    }

    Logger::instance()->info(QString("开始绘制矩形%1 请拖拽绘制矩形，右键完成")
                                 .arg(drawingType == "template" ? "模板" : "区域"));
}

void ImageView::finishRectangleDrawing()
{
    m_rectangleMode = false;
    m_currentRectDrawingType.clear();
}

void ImageView::clearRectangleDrawing()
{
    m_rectangleMode = false;
    m_currentRectDrawingType.clear();
    if (m_rectItem) {
        delete m_rectItem;
        m_rectItem = nullptr;
    }
}

// =================== 参考线绘制 ===================
void ImageView::startReferenceLineDrawing()
{
    m_referenceLineMode = true;
    m_referenceLineWaitingConfirm = false;
    m_refLineStartPosImg = QPointF();
    
    // 清除旧的参考线
    if (m_referenceLineItem) {
        delete m_referenceLineItem;
        m_referenceLineItem = nullptr;
    }
    if (m_refLinePreviewItem) {
        delete m_refLinePreviewItem;
        m_refLinePreviewItem = nullptr;
    }
    
    // 显示提示文字
    showReferenceLineHint("请左键点击设置参考线起点");
    
    Logger::instance()->info("参考线绘制模式已启动，请点击设置起点");
}

void ImageView::clearReferenceLine()
{
    m_referenceLineMode = false;
    m_referenceLineWaitingConfirm = false;
    m_refLineStartPosImg = QPointF();
    
    if (m_referenceLineItem) {
        delete m_referenceLineItem;
        m_referenceLineItem = nullptr;
    }
    if (m_refLinePreviewItem) {
        delete m_refLinePreviewItem;
        m_refLinePreviewItem = nullptr;
    }
    if (m_refLineHintItem) {
        delete m_refLineHintItem;
        m_refLineHintItem = nullptr;
    }
}

void ImageView::showReferenceLineHint(const QString& text)
{
    if (m_refLineHintItem) {
        delete m_refLineHintItem;
        m_refLineHintItem = nullptr;
    }
    
    m_refLineHintItem = new QGraphicsTextItem(m_pixmapItem);
    m_refLineHintItem->setPlainText(text);
    m_refLineHintItem->setDefaultTextColor(QColor(255, 200, 0));
    QFont f("Microsoft YaHei", 12);
    f.setBold(true);
    m_refLineHintItem->setFont(f);
    m_refLineHintItem->setPos(10, 10);
    m_refLineHintItem->setZValue(100);
}

void ImageView::resetZoom()
{
    if (!m_pixmapItem || m_pixmapItem->pixmap().isNull()) {
        return;
    }
    
    // 重置变换
    resetTransform();
    m_scaleFactor = 1.0;
    
    // 重新适应视图
    if (this->width() > 0 && this->height() > 0) {
        fitInView(m_pixmapItem, Qt::KeepAspectRatio);
        QTransform t = transform();
        m_scaleFactor = t.m11();
    }
}

void ImageView::clear()
{
    // 清空 pixmap
    m_pixmapItem->setPixmap(QPixmap());
    m_pixmapItem->setPos(0, 0);
    
    // 清空场景
    m_scene->setSceneRect(0, 0, 0, 0);
    
    // 清理 ROI
    clearRoi();
    
    // 清理多边形
    clearPolygon();
    
    // 清理矩形
    clearRectangleDrawing();
    
    // 清理参考线
    clearReferenceLine();
    
    // 重置缩放
    resetTransform();
    m_scaleFactor = 1.0;
}

// =================== 右键上下文菜单 ===================
void ImageView::showContextMenu(const QPoint& viewportPos)
{
    QMenu menu(this);
    QAction* fitAction = menu.addAction(QStringLiteral("适应窗口"));
    connect(fitAction, &QAction::triggered, this, &ImageView::resetZoom);
    menu.exec(mapToGlobal(viewportPos));
}
