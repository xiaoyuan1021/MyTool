#include "widgets/batch_match_dialog.h"
#include "roi_manager.h"
#include "logger.h"
#include "utils/path_utils.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QFileDialog>
#include <QMessageBox>
#include <QFile>
#include <QTextStream>
#include <QApplication>
#include <QtConcurrent/QtConcurrent>

BatchMatchDialog::BatchMatchDialog(RoiManager* roiManager,
                                   std::shared_ptr<IMatchStrategy> strategy,
                                   QWidget* parent)
    : QDialog(parent)
    , m_roiManager(roiManager)
    , m_strategy(strategy)
    , m_watcher(new QFutureWatcher<void>(this))
{
    setWindowTitle("批量模板匹配结果");
    setMinimumSize(900, 500);
    resize(1000, 600);

    setupUi();

    connect(m_watcher, &QFutureWatcher<void>::finished,
            this, &BatchMatchDialog::onProcessFinished);
}

void BatchMatchDialog::setupUi()
{
    auto* mainLayout = new QVBoxLayout(this);

    // ========== 顶部状态栏 ==========
    auto* topLayout = new QHBoxLayout();

    m_labelSummary = new QLabel("就绪 — 点击「开始匹配」遍历所有图片");
    m_labelElapsed = new QLabel("耗时: --");
    m_labelElapsed->setStyleSheet("color: #666;");

    topLayout->addWidget(m_labelSummary, 1);
    topLayout->addWidget(m_labelElapsed);
    mainLayout->addLayout(topLayout);

    // ========== 进度条 ==========
    m_progressBar = new QProgressBar();
    m_progressBar->setRange(0, 100);
    m_progressBar->setValue(0);
    m_progressBar->setTextVisible(true);
    mainLayout->addWidget(m_progressBar);

    // ========== 结果表格 ==========
    m_table = new QTableWidget(0, 7);
    m_table->setHorizontalHeaderLabels({"#", "图片名称", "匹配数", "最高分", "平均分", "结果", "状态"});
    m_table->horizontalHeader()->setStretchLastSection(true);
    m_table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setAlternatingRowColors(true);
    m_table->verticalHeader()->setVisible(false);

    connect(m_table, &QTableWidget::cellDoubleClicked,
            this, &BatchMatchDialog::onRowDoubleClicked);

    mainLayout->addWidget(m_table, 1);

    // ========== 底部按钮 ==========
    auto* btnLayout = new QHBoxLayout();

    m_btnStart = new QPushButton("开始匹配");
    m_btnStop = new QPushButton("停止");
    m_btnStop->setEnabled(false);
    m_btnExportCsv = new QPushButton("导出CSV");
    m_btnExportCsv->setEnabled(false);

    auto* btnClose = new QPushButton("关闭");

    btnLayout->addWidget(m_btnStart);
    btnLayout->addWidget(m_btnStop);
    btnLayout->addStretch();
    btnLayout->addWidget(m_btnExportCsv);
    btnLayout->addWidget(btnClose);

    mainLayout->addLayout(btnLayout);

    // ========== 信号连接 ==========
    connect(m_btnStart, &QPushButton::clicked, this, &BatchMatchDialog::onStartClicked);
    connect(m_btnStop, &QPushButton::clicked, this, &BatchMatchDialog::onStopClicked);
    connect(m_btnExportCsv, &QPushButton::clicked, this, &BatchMatchDialog::onExportCsvClicked);
    connect(btnClose, &QPushButton::clicked, this, &QDialog::accept);
}

// ========== 批量匹配控制 ==========

void BatchMatchDialog::startBatch(double minScore, int maxMatchesPerImage)
{
    m_minScore = minScore;
    m_maxMatches = maxMatchesPerImage;
    onStartClicked();
}

void BatchMatchDialog::onStartClicked()
{
    if (!m_strategy || !m_strategy->hasTemplate()) {
        QMessageBox::warning(this, "提示", "请先创建模板！");
        return;
    }

    QStringList imageIds = m_roiManager->getImageIds();
    if (imageIds.isEmpty()) {
        QMessageBox::warning(this, "提示", "没有图片可处理，请先添加图片。");
        return;
    }

    // 重置状态
    m_results.clear();
    for (const QString& id : imageIds) {
        BatchMatchItem item;
        item.imageId = id;
        item.imageName = m_roiManager->getImageName(id);
        item.imagePath = m_roiManager->getImageFilePath(id);
        m_results.append(item);
    }

    m_currentIndex = 0;
    m_running = true;
    m_stopRequested.storeRelaxed(0);
    m_elapsedTimer.start();

    // 更新UI状态
    m_btnStart->setEnabled(false);
    m_btnStop->setEnabled(true);
    m_btnExportCsv->setEnabled(false);
    m_progressBar->setRange(0, m_results.size());
    m_progressBar->setValue(0);

    updateTable();
    m_labelSummary->setText(QString("处理中... 0/%1").arg(m_results.size()));

    // 开始异步处理
    QFuture<void> future = QtConcurrent::run([this]() {
        processNextImage();
    });
    m_watcher->setFuture(future);
}

void BatchMatchDialog::onStopClicked()
{
    m_stopRequested.storeRelaxed(1);
    m_btnStop->setEnabled(false);
    m_labelSummary->setText("正在停止...");
}

void BatchMatchDialog::onProcessFinished()
{
    // 检查是否还有更多需要处理的图片
    if (m_stopRequested.loadRelaxed()) {
        finishBatch();
        return;
    }

    // 处理下一张
    processNextImage();
}

void BatchMatchDialog::processNextImage()
{
    if (m_stopRequested.loadRelaxed() || m_currentIndex >= m_results.size()) {
        // 回到主线程完成
        QMetaObject::invokeMethod(this, "finishBatch", Qt::QueuedConnection);
        return;
    }

    int idx = m_currentIndex;
    m_currentIndex++;

    BatchMatchItem item = processSingleImage(m_results[idx].imageId);

    // 在主线程更新结果
    QMetaObject::invokeMethod(this, [this, idx, item]() {
        if (idx < m_results.size()) {
            m_results[idx] = item;
            updateTable();
            m_progressBar->setValue(idx + 1);
            m_labelSummary->setText(QString("处理中... %1/%2 | 通过: %3 | 失败: %4")
                .arg(idx + 1)
                .arg(m_results.size())
                .arg(std::count_if(m_results.begin(), m_results.begin() + idx + 1,
                    [](const BatchMatchItem& i) { return i.passed; }))
                .arg(std::count_if(m_results.begin(), m_results.begin() + idx + 1,
                    [](const BatchMatchItem& i) { return !i.passed && i.processed; })));
            m_labelElapsed->setText(QString("耗时: %1s")
                .arg(m_elapsedTimer.elapsed() / 1000.0, 0, 'f', 1));
        }
    }, Qt::QueuedConnection);

    // 异步继续处理下一张
    QFuture<void> future = QtConcurrent::run([this]() {
        processNextImage();
    });
    m_watcher->setFuture(future);
}

BatchMatchItem BatchMatchDialog::processSingleImage(const QString& imageId)
{
    BatchMatchItem result;
    result.imageId = imageId;
    result.imageName = m_roiManager->getImageName(imageId);
    result.imagePath = m_roiManager->getImageFilePath(imageId);

    try {
        // 加载图片：优先从文件加载，失败则从RoiManager内存中获取
        cv::Mat img;
        if (!result.imagePath.isEmpty()) {
            img = PathUtils::readImageFromFile(result.imagePath, cv::IMREAD_COLOR);
        }
        if (img.empty()) {
            img = m_roiManager->getImage(imageId);
        }
        if (img.empty()) {
            result.statusText = "无法加载图片";
            result.processed = true;
            result.passed = false;
            return result;
        }

        // 执行模板匹配
        QVector<MatchResult> matches = m_strategy->findMatches(
            img, m_minScore, m_maxMatches, 0.7);

        result.matchCount = matches.size();
        result.matches = matches;

        if (!matches.isEmpty()) {
            double totalScore = 0;
            for (const auto& m : matches) {
                totalScore += m.score;
            }
            result.maxScore = matches.first().score;
            result.avgScore = totalScore / matches.size();
        }

        result.passed = (result.maxScore >= m_minScore);
        result.processed = true;

        if (matches.isEmpty()) {
            result.statusText = "未找到匹配";
        } else {
            result.statusText = QString("找到 %1 个匹配, 最高分: %2")
                .arg(matches.size())
                .arg(result.maxScore, 0, 'f', 3);
        }

        // 生成结果图像
        result.resultImage = m_strategy->drawMatches(img.clone(), matches);

    } catch (const cv::Exception& e) {
        result.statusText = QString("OpenCV错误: %1").arg(e.what());
        result.processed = true;
        result.passed = false;
    } catch (const std::exception& e) {
        result.statusText = QString("错误: %1").arg(e.what());
        result.processed = true;
        result.passed = false;
    }

    return result;
}

void BatchMatchDialog::finishBatch()
{
    m_running = false;
    m_btnStart->setEnabled(true);
    m_btnStop->setEnabled(false);

    int total = m_results.size();
    int passed = 0;
    int failed = 0;
    for (const auto& r : m_results) {
        if (r.processed) {
            if (r.passed) passed++;
            else failed++;
        }
    }

    double elapsed = m_elapsedTimer.elapsed() / 1000.0;
    m_labelSummary->setText(
        QString("处理完成 | 总计: %1 | 通过: %2 | 未通过: %3 | 通过率: %4% | 耗时: %5s")
            .arg(total).arg(passed).arg(failed)
            .arg(total > 0 ? passed * 100.0 / total : 0.0, 0, 'f', 1)
            .arg(elapsed, 0, 'f', 1));
    m_labelElapsed->setText(QString("耗时: %1s").arg(elapsed, 0, 'f', 1));
    m_progressBar->setValue(total);

    m_btnExportCsv->setEnabled(total > 0);

    Logger::instance()->info(QString("[BatchMatch] 批量匹配完成: 共%1张, 通过%2, 未通过%3, 耗时%4s")
        .arg(total).arg(passed).arg(failed).arg(elapsed, 0, 'f', 1));
}

// ========== 表格显示 ==========

void BatchMatchDialog::updateTable()
{
    m_table->setRowCount(m_results.size());

    for (int i = 0; i < m_results.size(); ++i) {
        const auto& item = m_results[i];

        auto* numItem = new QTableWidgetItem(QString::number(i + 1));
        numItem->setTextAlignment(Qt::AlignCenter);
        m_table->setItem(i, 0, numItem);

        m_table->setItem(i, 1, new QTableWidgetItem(item.imageName));

        auto* matchItem = new QTableWidgetItem(
            item.processed ? QString::number(item.matchCount) : "--");
        matchItem->setTextAlignment(Qt::AlignCenter);
        m_table->setItem(i, 2, matchItem);

        auto* scoreItem = new QTableWidgetItem(
            item.processed && item.matchCount > 0
                ? QString::number(item.maxScore, 'f', 4)
                : "--");
        scoreItem->setTextAlignment(Qt::AlignCenter);
        m_table->setItem(i, 3, scoreItem);

        auto* avgItem = new QTableWidgetItem(
            item.processed && item.matchCount > 0
                ? QString::number(item.avgScore, 'f', 4)
                : "--");
        avgItem->setTextAlignment(Qt::AlignCenter);
        m_table->setItem(i, 4, avgItem);

        QString resultText;
        QColor resultColor;
        if (!item.processed) {
            resultText = "待处理";
            resultColor = QColor("#999");
        } else if (item.passed) {
            resultText = "PASS";
            resultColor = QColor("#0a0");
        } else {
            resultText = "FAIL";
            resultColor = QColor("#d00");
        }
        auto* resultItem = new QTableWidgetItem(resultText);
        resultItem->setForeground(resultColor);
        resultItem->setTextAlignment(Qt::AlignCenter);
        m_table->setItem(i, 5, resultItem);

        m_table->setItem(i, 6, new QTableWidgetItem(
            item.processed ? item.statusText : "等待处理..."));
    }

    m_table->resizeColumnsToContents();
    // 图片名称列自动拉伸
    m_table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
}

// ========== 导出CSV ==========

void BatchMatchDialog::onExportCsvClicked()
{
    QString filePath = QFileDialog::getSaveFileName(
        this, "导出CSV结果", "batch_match_results.csv",
        "CSV文件 (*.csv)");

    if (filePath.isEmpty()) return;

    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::warning(this, "错误", "无法写入文件: " + filePath);
        return;
    }

    QTextStream out(&file);
    // UTF-8 BOM
    out.setEncoding(QStringConverter::Utf8);
    out << QChar(0xFEFF);

    out << "序号,图片名称,匹配数,最高分,平均分,结果,状态\n";
    for (int i = 0; i < m_results.size(); ++i) {
        const auto& item = m_results[i];
        out << (i + 1) << ","
            << "\"" << item.imageName << "\","
            << (item.processed ? QString::number(item.matchCount) : "N/A") << ","
            << (item.processed && item.matchCount > 0 ? QString::number(item.maxScore, 'f', 4) : "N/A") << ","
            << (item.processed && item.matchCount > 0 ? QString::number(item.avgScore, 'f', 4) : "N/A") << ","
            << (item.processed ? (item.passed ? "PASS" : "FAIL") : "PENDING") << ","
            << "\"" << (item.processed ? item.statusText : "") << "\"\n";
    }

    file.close();
    Logger::instance()->info(QString("[BatchMatch] 结果已导出: %1").arg(filePath));
    QMessageBox::information(this, "导出完成",
        QString("结果已导出到:\n%1").arg(filePath));
}

// ========== 双击查看详情 ==========

void BatchMatchDialog::onRowDoubleClicked(int row, int column)
{
    Q_UNUSED(column);
    if (row < 0 || row >= m_results.size()) return;

    const BatchMatchItem& item = m_results[row];
    if (!item.processed) return;

    emit viewResultRequested(item.imageId, item.resultImage, item.matches);
}
