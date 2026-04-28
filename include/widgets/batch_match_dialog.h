#pragma once

#include <QDialog>
#include <QTableWidget>
#include <QProgressBar>
#include <QLabel>
#include <QPushButton>
#include <QElapsedTimer>
#include <QAtomicInt>
#include <QFutureWatcher>
#include <memory>
#include <opencv2/opencv.hpp>
#include "algorithm/match_strategy.h"

class RoiManager;

/**
 * 单张图片的模板匹配结果
 */
struct BatchMatchItem
{
    QString imageId;
    QString imageName;
    QString imagePath;
    int matchCount = 0;
    double maxScore = 0.0;
    double avgScore = 0.0;
    bool passed = false;
    bool processed = false;
    QString statusText;
    QVector<MatchResult> matches;
    cv::Mat resultImage;
};

/**
 * 批量模板匹配对话框
 *
 * 遍历 RoiManager 中的所有图片，对每张图执行当前模板匹配策略，
 * 以表格形式展示结果，支持双击查看详情和导出CSV。
 */
class BatchMatchDialog : public QDialog
{
    Q_OBJECT

public:
    explicit BatchMatchDialog(RoiManager* roiManager,
                              std::shared_ptr<IMatchStrategy> strategy,
                              QWidget* parent = nullptr);
    ~BatchMatchDialog() = default;

    /// 开始批量匹配
    void startBatch(double minScore, int maxMatchesPerImage);

    /// 获取结果列表
    const QVector<BatchMatchItem>& results() const { return m_results; }

signals:
    /// 用户双击某行，请求查看该图片的匹配详情
    void viewResultRequested(const QString& imageId, const cv::Mat& resultImage,
                             const QVector<MatchResult>& matches);

private slots:
    void onStartClicked();
    void onStopClicked();
    void onExportCsvClicked();
    void onRowDoubleClicked(int row, int column);
    void onProcessFinished();

private:
    void setupUi();
    void updateTable();
    void processNextImage();
    void finishBatch();
    BatchMatchItem processSingleImage(const QString& imageId);

    RoiManager* m_roiManager;
    std::shared_ptr<IMatchStrategy> m_strategy;

    double m_minScore = 0.8;
    int m_maxMatches = 1;

    QVector<BatchMatchItem> m_results;
    int m_currentIndex = 0;
    bool m_running = false;
    QAtomicInt m_stopRequested{0};
    QElapsedTimer m_elapsedTimer;

    // UI控件
    QTableWidget* m_table = nullptr;
    QProgressBar* m_progressBar = nullptr;
    QLabel* m_labelSummary = nullptr;
    QPushButton* m_btnStart = nullptr;
    QPushButton* m_btnStop = nullptr;
    QPushButton* m_btnExportCsv = nullptr;
    QLabel* m_labelElapsed = nullptr;

    // 异步处理
    QFutureWatcher<void>* m_watcher = nullptr;
};
