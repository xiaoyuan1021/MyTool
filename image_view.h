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

class ImageView : public QGraphicsView
{
    Q_OBJECT
public:
    explicit ImageView(QWidget* parent = nullptr);
    void setImage(const QImage &img);
    void setRoiMode(bool enable);
    void clearRoi();
    void finishRoiMode();

    void setPolygonMode(bool enable);
    void clearPolygon();

    void startPolygonDrawing(const QString& drawingType);
    void finishPolygonDrawing();
    void clearPolygonDrawing();

    QVector<QPointF> getPolygonPoints() const;

signals:
    void pixelInfoChanged(int x,int y, const QColor& rgb,int gray);
    void roiSelected(const QRectF &roiRect);   // 发出的是图像坐标系中的 ROI

    void polygonPointAdded(const QPointF& point);
    void polygonFinished(const QVector<QPointF>& points);

    void polygonDrawingPointAdded(const QString& type,const QPointF& point);
    void polygonDrawingFinished(const QString& type,QVector<QPointF> points);

protected:
    void wheelEvent(QWheelEvent * event) override;
    void mouseMoveEvent(QMouseEvent * event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;

private:
    // 把视图坐标转换为“图像坐标”（pixmapItem 本地坐标）
    QPointF viewPosToImagePos(const QPoint &viewPos) const;

    QGraphicsScene *m_scene;
    QGraphicsPixmapItem *m_pixmapItem;
    QImage m_image;
    double m_scaleFactor = 1.0;

    bool m_isDrawingRoi = false;
    QPointF m_roiStartPosImg;              // ⭐ 起点：图像坐标
    QGraphicsRectItem *m_roiRectItem = nullptr; // 子项，挂在 m_pixmapItem 上
    bool m_roiMode = false;
    bool m_viewMode;
    // 最近一次绘制完成的 ROI（图像坐标 = pixmapItem本地坐标）
    QRectF m_roiRectImg;
    // ROI 已绘制完成，等待右键确认裁剪
    bool   m_roiReady = false;

    bool m_polygonMode;
    QVector <QPointF> m_polygonPoints;
    QGraphicsPathItem * m_polygonPathItem;
    void updatePolygonDisplay();

    QString m_currentDrawingType;
    void updatePolygonPath(const QVector<QPointF>& points, QGraphicsPathItem*& pathItem);



};

class RoiManager
{
public:
    RoiManager()=default;

    void setFullImage(const cv::Mat & img);
    cv::Mat getCurrentImage() const;
    cv::Mat getFullImage() const;
    bool applyRoi(const QRectF& roiRectF);
    void resetRoi();
    bool isRoiActive() const;
    cv::Rect getLastRoi() const;
    void clear();

private:
    cv::Mat m_fullImage;
    cv::Mat m_currentImage;
    bool m_isRoiActive=false;
    cv::Rect m_lastRoi;
};

#endif // IMAGE_VIEW_H
