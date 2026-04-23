#ifndef IMAGE_LIST_MANAGER_H
#define IMAGE_LIST_MANAGER_H

#include <QObject>
#include <QListWidget>
#include <QPushButton>
#include <QLabel>
#include <QMessageBox>
#include <QFileInfo>

#include <opencv2/opencv.hpp>

#include "roi_manager.h"
#include "file_manager.h"
#include "logger.h"

/**
 * 图片列表管理器 - 负责多图片列表的UI交互和管理
 *
 * 职责：
 * 1. 图片列表的显示和刷新
 * 2. 图片的添加/删除操作
 * 3. 图片切换时的选中状态同步
 */
class ImageListManager : public QObject
{
    Q_OBJECT

public:
    explicit ImageListManager(
        RoiManager& roiManager,
        FileManager* fileManager,
        QWidget* parentWidget,
        QObject* parent = nullptr
    );

    /**
     * @brief 初始化：绑定UI控件并连接信号
     */
    void init(QListWidget* listWidget, QPushButton* addBtn, QPushButton* removeBtn);

    /**
     * @brief 刷新整个图片列表
     */
    void refresh();

signals:
    /**
     * @brief 图片切换后需要显示图像
     */
    void imageDisplayRequested(const cv::Mat& img);

    /**
     * @brief 需要重新处理图像
     */
    void processRequested();

private slots:
    void onAddClicked();
    void onRemoveClicked();
    void onSelectionChanged();

    void onImageAdded(const QString& imageId);
    void onImageRemoved(const QString& imageId);
    void onCurrentImageChanged(const QString& imageId);

private:
    RoiManager& m_roiManager;
    FileManager* m_fileManager;
    QWidget* m_parentWidget;

    QListWidget* m_listWidget = nullptr;
};

#endif // IMAGE_LIST_MANAGER_H