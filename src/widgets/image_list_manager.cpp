#include "widgets/image_list_manager.h"
#include "image_utils.h"

ImageListManager::ImageListManager(
    RoiManager& roiManager,
    FileManager* fileManager,
    QWidget* parentWidget,
    QObject* parent)
    : QObject(parent)
    , m_roiManager(roiManager)
    , m_fileManager(fileManager)
    , m_parentWidget(parentWidget)
{
}

void ImageListManager::init(QListWidget* listWidget, QPushButton* addBtn, QPushButton* removeBtn)
{
    m_listWidget = listWidget;

    connect(addBtn, &QPushButton::clicked, this, &ImageListManager::onAddClicked);
    connect(removeBtn, &QPushButton::clicked, this, &ImageListManager::onRemoveClicked);
    connect(m_listWidget, &QListWidget::itemSelectionChanged, this, &ImageListManager::onSelectionChanged);

    connect(&m_roiManager, &RoiManager::imageAdded, this, &ImageListManager::onImageAdded);
    connect(&m_roiManager, &RoiManager::imageRemoved, this, &ImageListManager::onImageRemoved);
    connect(&m_roiManager, &RoiManager::currentImageChanged, this, &ImageListManager::onCurrentImageChanged);

    refresh();
}

void ImageListManager::refresh()
{
    m_listWidget->clear();

    QStringList imageIds = m_roiManager.getImageIds();
    for (const QString& imageId : imageIds) {
        QString imageName = m_roiManager.getImageName(imageId);
        QListWidgetItem* item = new QListWidgetItem(imageName);
        item->setData(Qt::UserRole, imageId);

        if (imageId == m_roiManager.getCurrentImageId()) {
            item->setSelected(true);
        }

        m_listWidget->addItem(item);
    }
}

void ImageListManager::onAddClicked()
{
    QString fileName = m_fileManager->selectImageFile("");
    if (fileName.isEmpty()) {
        return;
    }

    cv::Mat img = cv::imread(fileName.toStdString());
    if (img.empty()) {
        QMessageBox::warning(m_parentWidget, "错误", "无法加载图片文件");
        return;
    }

    QFileInfo fileInfo(fileName);
    QString imageName = fileInfo.completeBaseName();
    QString imageId = m_roiManager.addImage(img, imageName);

    if (!imageId.isEmpty()) {
        m_roiManager.switchToImage(imageId);

        const cv::Mat& currentImage = m_roiManager.getFullImage();
        if (!currentImage.empty()) {
            emit imageDisplayRequested(currentImage);
        }
    }
}

void ImageListManager::onRemoveClicked()
{
    QString currentImageId = m_roiManager.getCurrentImageId();
    if (currentImageId.isEmpty()) {
        QMessageBox::information(m_parentWidget, "提示", "没有可删除的图片");
        return;
    }

    QString imageName = m_roiManager.getImageName(currentImageId);
    int ret = QMessageBox::question(m_parentWidget, "确认删除",
                                   QString("确定要删除图片 '%1' 吗？").arg(imageName),
                                   QMessageBox::Yes | QMessageBox::No);

    if (ret == QMessageBox::Yes) {
        m_roiManager.removeImage(currentImageId);

        QStringList imageIds = m_roiManager.getImageIds();
        if (!imageIds.isEmpty()) {
            m_roiManager.switchToImage(imageIds.first());
        } else {
            emit imageDisplayRequested(cv::Mat());
        }
    }
}

void ImageListManager::onSelectionChanged()
{
    QListWidgetItem* currentItem = m_listWidget->currentItem();
    if (!currentItem) {
        return;
    }

    QString imageId = currentItem->data(Qt::UserRole).toString();
    if (!imageId.isEmpty() && imageId != m_roiManager.getCurrentImageId()) {
        m_roiManager.switchToImage(imageId);
    }
}

void ImageListManager::onImageAdded(const QString& imageId)
{
    QString imageName = m_roiManager.getImageName(imageId);
    QListWidgetItem* item = new QListWidgetItem(imageName);
    item->setData(Qt::UserRole, imageId);
    m_listWidget->addItem(item);

    Logger::instance()->info(QString("图片已添加到列表: %1").arg(imageName));
}

void ImageListManager::onImageRemoved(const QString& imageId)
{
    for (int i = 0; i < m_listWidget->count(); ++i) {
        QListWidgetItem* item = m_listWidget->item(i);
        if (item->data(Qt::UserRole).toString() == imageId) {
            delete item;
            break;
        }
    }

    Logger::instance()->info("图片已从列表中移除");
}

void ImageListManager::onCurrentImageChanged(const QString& imageId)
{
    for (int i = 0; i < m_listWidget->count(); ++i) {
        QListWidgetItem* item = m_listWidget->item(i);
        bool isSelected = (item->data(Qt::UserRole).toString() == imageId);
        item->setSelected(isSelected);
    }

    const cv::Mat& currentImage = m_roiManager.getFullImage();
    if (!currentImage.empty()) {
        emit imageDisplayRequested(currentImage);
    }

    Logger::instance()->info(QString("已切换到图片: %1").arg(imageId));
}