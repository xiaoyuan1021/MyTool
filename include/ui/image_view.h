// imagelabel.h
#ifndef IMAGE_VIEW_H
#define IMAGE_VIEW_H

#include <QGraphicsScene>
#include <QGraphicsView>
#include <QGraphicsPixmapItem>
#include <QLabel>
#include <QDebug>
#include <QImage>
#include <QPixmap>
#include <QPainter>
#include <QWheelEvent>
#include <QMouseEvent>

#include <opencv2/opencv.hpp>
#include "logger.h"
#include "roi_manager.h"

// ROI 交互状态
enum RoiHandle
{
    None,        // 不在ROI上
    Move,        // 拖拽整个ROI
    TopLeft,     // 左上角调整
    TopRight,    // 右上角调整
    BottomLeft,  // 左下角调整
    BottomRight, // 右下角调整
    Top,         // 上边缘调整
    Bottom,      // 下边缘调整
    Left,        // 左边缘调整
    Right        // 右边缘调整
};

class ImageView : public QGraphicsView
{
    Q_OBJECT
public:
    explicit ImageView(QWidget *parent = nullptr);
    void setImage(const QImage &img);
    void setRoiMode(bool enable);
    void clearRoi();
    void finishRoiMode();

    void setPolygonMode(bool enable);
    void clearPolygon();

    void startPolygonDrawing(const QString &drawingType);
    void finishPolygonDrawing();
    void clearPolygonDrawing();

    void startRectangleDrawing(const QString &drawingType);
    void finishRectangleDrawing();
    void clearRectangleDrawing();

    // 参考线绘制
    void startReferenceLineDrawing();
    void clearReferenceLine();

    QVector<QPointF> getPolygonPoints() const;

    // 重置缩放到原始比例
    void resetZoom();


signals:
    void pixelInfoChanged(int x, int y, const QColor &rgb, int gray);
    void roiSelected(const QRectF &roiRect); // 发出的是图像坐标系中的 ROI

    void polygonPointAdded(const QPointF &point);
    void polygonFinished(const QVector<QPointF> &points);

    void polygonDrawingPointAdded(const QString &type, const QPointF &point);
    void polygonDrawingFinished(const QString &type, QVector<QPointF> points);
    
    // 参考线绘制完成信号
    void referenceLineDrawn(const cv::Point2f& start, const cv::Point2f& end);

protected:
    void wheelEvent(QWheelEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;

private:
    RoiHandle m_roiHandle = None; // 当前激活的把手
    // 检测鼠标在ROI框上的位置
    RoiHandle getRoiHandleAtPos(const QPointF &imgPos) const;
    void setCursorForHandle(RoiHandle handle);
    QPointF m_dragStartPos;       // 拖拽起始位置
    QRectF m_dragStartRect;       // 拖拽起始时的ROI矩形
    int m_handleSize = 10;        // 把手大小（像素）

    // 把视图坐标转换为“图像坐标”（pixmapItem 本地坐标）
    QPointF viewPosToImagePos(const QPoint &viewPos) const;

    QGraphicsScene *m_scene;
    QGraphicsPixmapItem *m_pixmapItem;
    // QImage m_image;
    QSize getImageSize() const
    {
        QPixmap pixmap = m_pixmapItem->pixmap();
        return pixmap.isNull() ? QSize(0, 0) : pixmap.size();
    }
    double m_scaleFactor = 1.0;

    bool m_isDrawingRoi = false;
    QPointF m_roiStartPosImg;                   // ⭐ 起点：图像坐标
    QGraphicsRectItem *m_roiRectItem = nullptr; // 子项，挂在 m_pixmapItem 上
    bool m_roiMode = false;
    bool m_viewMode;
    // 最近一次绘制完成的 ROI（图像坐标 = pixmapItem本地坐标）
    QRectF m_roiRectImg;
    // ROI 已绘制完成，等待右键确认裁剪
    bool m_roiReady = false;

    bool m_polygonMode;
    QVector<QPointF> m_polygonPoints;
    QGraphicsPathItem *m_polygonPathItem;
    void updatePolygonDisplay();

    QString m_currentDrawingType;
    void updatePolygonPath(const QVector<QPointF> &points, QGraphicsPathItem *&pathItem);

    bool m_rectangleMode = false;
    QPointF m_rectStartPosImg;
    QGraphicsRectItem *m_rectItem = nullptr;
    QString m_currentRectDrawingType;
    
    // 参考线绘制
    bool m_referenceLineMode = false;
    QPointF m_refLineStartPosImg;
    QGraphicsLineItem *m_referenceLineItem = nullptr;
};

#endif // IMAGE_VIEW_H
