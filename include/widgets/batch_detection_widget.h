#pragma once

#include <QWidget>
#include <QFutureWatcher>
#include <QElapsedTimer>
#include <QAtomicInt>
#include <QVector>
#include <opencv2/opencv.hpp>

namespace Ui {
class BatchDetectionWidget;
}

class RoiManager;
class PipelineManager;
class ProfileManager;
struct InspectionProfile;
struct DetectionResultReport;

/**
 * @brief 单张图片的批量检测结果
 */
struct BatchDetectionItem
{
    QString imageId;
    QString imageName;
    QString imagePath;
    bool passed = false;
    bool processed = false;
    QString statusText;
    QList<DetectionResultReport> roiReports;  ///< 每个ROI的检测报告
};

/**
 * @brief 统一的批量检测面板
 *
 * 整合了原有的 AutoDetectionController 和 BatchMatchDialog 功能：
 * - 支持所有检测类型（Blob/Line/Barcode/Template/ObjectDetection）
 * - 可选择使用当前配置或已保存的方案
 * - 实时进度、结果表格、导出报告
 */
class BatchDetectionWidget : public QWidget
{
    Q_OBJECT

public:
    explicit BatchDetectionWidget(RoiManager* roiManager,
                                  PipelineManager* pipelineManager,
                                  ProfileManager* profileManager,
                                  QWidget* parent = nullptr);
    ~BatchDetectionWidget();

    /// 开始批量检测
    void startDetection(const QString& profileId = QString());

    /// 停止检测
    void stopDetection();

    /// 暂停/恢复
    void pauseDetection();
    void resumeDetection();

    /// 获取结果
    const QVector<BatchDetectionItem>& results() const { return m_results; }

signals:
    void detectionStarted(int totalCount);
    void detectionFinished(int passCount, int failCount);
    void progressUpdated(int current, int total, const QString& imageName);
    void imageResultReady(int index, const QString& imageName, bool passed);
    void logMessage(const QString& message);

private slots:
    void onStartClicked();
    void onStopClicked();
    void onPauseClicked();
    void onExportCsvClicked();
    void onRowDoubleClicked(int row, int column);
    void onProfileComboChanged(int index);

private:
    void setupCustomUi();
    void setupConnections();
    void updateUiState(bool running);
    void processNextImage();
    void processSingleImage(const QString& imageId);
    void finishDetection();
    void updateProfileCombo();

    Ui::BatchDetectionWidget* m_ui;
    RoiManager* m_roiManager;
    PipelineManager* m_pipelineManager;
    ProfileManager* m_profileManager;

    // 检测状态
    QVector<BatchDetectionItem> m_results;
    int m_currentIndex = 0;
    bool m_running = false;
    bool m_paused = false;
    bool m_stopRequested = false;
    QAtomicInt m_pauseFlag{0};

    // 当前使用的方案ID
    QString m_currentProfileId;

    QElapsedTimer m_elapsedTimer;
};