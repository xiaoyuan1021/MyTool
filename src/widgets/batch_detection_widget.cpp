#include "batch_detection_widget.h"
#include "ui_batch_detection_tab.h"
#include "roi_manager.h"
#include "core/pipeline_manager.h"
#include "core/profile_manager.h"
#include "data/inspection_profile.h"
#include "data/detection_result_report.h"
#include "config/detection_config_types.h"
#include "algorithm/image_utils.h"
#include "ui/logger.h"
#include <QFileDialog>
#include <QMessageBox>
#include <QFile>
#include <QTextStream>
#include <QFileInfo>
#include <QtConcurrent/QtConcurrent>
#include <QCoreApplication>

BatchDetectionWidget::BatchDetectionWidget(
    RoiManager* roiManager,
    PipelineManager* pipelineManager,
    ProfileManager* profileManager,
    QWidget* parent)
    : QWidget(parent)
    , m_ui(new Ui::BatchDetectionWidget)
    , m_roiManager(roiManager)
    , m_pipelineManager(pipelineManager)
    , m_profileManager(profileManager)
{
    m_ui->setupUi(this);
    setupCustomUi();
    setupConnections();
    updateProfileCombo();
}

BatchDetectionWidget::~BatchDetectionWidget()
{
    delete m_ui;
}

// ==================== UI设置 ====================

void BatchDetectionWidget::setupCustomUi()
{
    // 表格列宽自适应
    m_ui->tableWidget_results->horizontalHeader()->setStretchLastSection(true);
}

void BatchDetectionWidget::setupConnections()
{
    connect(m_ui->btn_start, &QPushButton::clicked,
            this, &BatchDetectionWidget::onStartClicked);
    connect(m_ui->btn_stop, &QPushButton::clicked,
            this, &BatchDetectionWidget::onStopClicked);
    connect(m_ui->btn_pause, &QPushButton::clicked,
            this, &BatchDetectionWidget::onPauseClicked);
    connect(m_ui->btn_exportCsv, &QPushButton::clicked,
            this, &BatchDetectionWidget::onExportCsvClicked);
    connect(m_ui->tableWidget_results, &QTableWidget::cellDoubleClicked,
            this, &BatchDetectionWidget::onRowDoubleClicked);
    connect(m_ui->comboBox_profile, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &BatchDetectionWidget::onProfileComboChanged);
    connect(m_ui->btn_addImageFolder, &QPushButton::clicked, this, [this]() {
        QString dir = QFileDialog::getExistingDirectory(
            this, "选择图片文件夹", QString(),
            QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);

        if (!dir.isEmpty()) {
            QStringList ids = m_roiManager->importImagesFromFolder(dir);
            if (!ids.isEmpty()) {
                m_ui->label_status->setText(
                    QString("已导入 %1 张图片").arg(ids.size()));
                emit logMessage(QString("从文件夹导入 %1 张图片: %2").arg(ids.size()).arg(dir));
            } else {
                QMessageBox::information(this, "导入结果", "未找到支持的图片文件");
            }
        }
    });
}

void BatchDetectionWidget::updateProfileCombo()
{
    m_ui->comboBox_profile->blockSignals(true);
    m_ui->comboBox_profile->clear();
    m_ui->comboBox_profile->addItem("（使用当前配置）", QString());

    if (m_profileManager) {
        QList<ProfileManager::ProfileSummary> profiles = m_profileManager->getProfileList();
        for (const auto& p : profiles) {
            m_ui->comboBox_profile->addItem(
                QString("📋 %1 (ROI:%2, 模板:%3)")
                    .arg(p.profileName).arg(p.roiCount).arg(p.templateCount),
                p.profileId);
        }
    }

    m_ui->comboBox_profile->blockSignals(false);
}

// ==================== 控制接口 ====================

void BatchDetectionWidget::startDetection(const QString& profileId)
{
    if (m_running) {
        QMessageBox::information(this, "提示", "检测已在运行中");
        return;
    }

    QStringList imageIds = m_roiManager->getImageIds();
    if (imageIds.isEmpty()) {
        QMessageBox::warning(this, "提示", "没有要检测的图片，请先导入图片");
        return;
    }

    // 如果指定了方案，先应用方案到所有图片
    if (!profileId.isEmpty() && m_profileManager) {
        InspectionProfile profile = m_profileManager->getProfile(profileId);
        if (!profile.profileName.isEmpty()) {
            // 应用方案到当前图片
            cv::Mat currentImage = m_roiManager->getFullImage();
            if (!currentImage.empty()) {
                m_roiManager->applyProfile(profile, QSize(currentImage.cols, currentImage.rows));
            }
            m_currentProfileId = profileId;
        }
    }

    // 构建结果列表
    m_results.clear();
    m_currentIndex = 0;
    m_stopRequested = false;
    m_paused = false;
    m_pauseFlag.storeRelease(0);
    m_running = true;

    for (const QString& imageId : imageIds) {
        BatchDetectionItem item;
        item.imageId = imageId;
        item.imageName = m_roiManager->getImageName(imageId);
        item.imagePath = m_roiManager->getImageFilePath(imageId);
        m_results.append(item);
    }

    // 清空表格
    m_ui->tableWidget_results->setRowCount(0);

    updateUiState(true);
    m_elapsedTimer.start();

    emit detectionStarted(m_results.size());
    emit logMessage(QString("开始批量检测，共 %1 张图片").arg(m_results.size()));

    // 开始处理第一张
    processNextImage();
}

void BatchDetectionWidget::stopDetection()
{
    m_stopRequested = true;
    m_pauseFlag.storeRelease(0);
    m_paused = false;
}

void BatchDetectionWidget::pauseDetection()
{
    m_paused = true;
    m_pauseFlag.storeRelease(1);
    m_ui->btn_pause->setText("▶ 继续");
    m_ui->label_status->setText("已暂停");
}

void BatchDetectionWidget::resumeDetection()
{
    m_paused = false;
    m_pauseFlag.storeRelease(0);
    m_ui->btn_pause->setText("⏸ 暂停");
    processNextImage();
}

// ==================== 槽函数 ====================

void BatchDetectionWidget::onStartClicked()
{
    QString profileId = m_ui->comboBox_profile->currentData().toString();
    startDetection(profileId);
}

void BatchDetectionWidget::onStopClicked()
{
    stopDetection();
}

void BatchDetectionWidget::onPauseClicked()
{
    if (m_paused) {
        resumeDetection();
    } else {
        pauseDetection();
    }
}

void BatchDetectionWidget::onProfileComboChanged(int index)
{
    Q_UNUSED(index);
    // 方案切换时可以预览方案详情（可扩展）
}

void BatchDetectionWidget::onRowDoubleClicked(int row, int column)
{
    Q_UNUSED(column);
    if (row < 0 || row >= m_results.size()) return;

    // 可扩展：双击查看结果详情
    const BatchDetectionItem& item = m_results[row];
    if (!item.processed) return;

    QString detail;
    for (const auto& report : item.roiReports) {
        detail += report.toSummaryString() + "\n";
    }
    if (detail.isEmpty()) {
        detail = item.statusText;
    }

    QMessageBox::information(this,
        QString("检测详情 - %1").arg(item.imageName),
        detail);
}

// ==================== 核心处理 ====================

void BatchDetectionWidget::processNextImage()
{
    if (m_stopRequested) {
        finishDetection();
        return;
    }

    if (m_paused) {
        return;
    }

    if (m_currentIndex >= m_results.size()) {
        finishDetection();
        return;
    }

    BatchDetectionItem& item = m_results[m_currentIndex];
    QString imageId = item.imageId;
    QString imagePath = item.imagePath;
    QString imageName = item.imageName;

    if (imagePath.isEmpty()) {
        item.statusText = "无文件路径，跳过";
        item.processed = true;
        m_currentIndex++;
        processNextImage();
        return;
    }

    QList<RoiConfig> roiConfigs = m_roiManager->getRoiConfigsForImage(imageId);
    if (roiConfigs.isEmpty()) {
        item.statusText = "无ROI配置，跳过";
        item.processed = true;
        m_currentIndex++;
        processNextImage();
        return;
    }

    // 更新状态
    m_ui->label_status->setText(
        QString("正在检测: %1 (%2/%3)")
            .arg(imageName).arg(m_currentIndex + 1).arg(m_results.size()));
    m_ui->progressBar->setValue(
        static_cast<int>((m_currentIndex * 100.0) / m_results.size()));

    // 在后台线程执行检测
    QPointer<PipelineManager> pipelinePtr = m_pipelineManager;
    int idx = m_currentIndex;

    QFuture<PipelineContext> future = QtConcurrent::run(
        [pipelinePtr, imagePath, roiConfigs]() -> PipelineContext {
            try {
                if (!pipelinePtr) {
                    PipelineContext emptyCtx;
                    emptyCtx.pass = false;
                    emptyCtx.reason = "PipelineManager已被释放";
                    return emptyCtx;
                }

                cv::Mat image = cv::imread(imagePath.toStdString());
                if (image.empty()) {
                    PipelineContext emptyCtx;
                    emptyCtx.pass = false;
                    emptyCtx.reason = QString("无法加载图片: %1").arg(imagePath);
                    return emptyCtx;
                }

                PipelineConfig savedGlobalConfig = pipelinePtr->getConfigSnapshot();

                bool allPassed = true;
                QString failReason;

                for (const RoiConfig& roiConfig : roiConfigs) {
                    if (!roiConfig.isActive) continue;

                    PipelineConfig config = savedGlobalConfig;
                    config.brightness = roiConfig.pipelineConfig.brightness;
                    config.contrast = roiConfig.pipelineConfig.contrast;
                    config.gamma = roiConfig.pipelineConfig.gamma;
                    config.sharpen = roiConfig.pipelineConfig.sharpen;
                    config.channel = roiConfig.pipelineConfig.channel;

                    cv::Rect roiRect = ImageUtils::mapRoiToCvRect(
                        roiConfig.roiRect, image.cols, image.rows);
                    if (roiRect.empty()) continue;

                    cv::Mat roiImage = image(roiRect).clone();
                    PipelineContext ctx = pipelinePtr->execute(roiImage, config);

                    if (!ctx.pass) {
                        allPassed = false;
                        failReason += QString("[%1] %2; ").arg(roiConfig.roiName, ctx.reason);
                    }
                }

                PipelineContext result;
                result.pass = allPassed;
                result.reason = failReason;
                return result;

            } catch (const std::exception& e) {
                PipelineContext emptyCtx;
                emptyCtx.pass = false;
                emptyCtx.reason = QString("异常: %1").arg(e.what());
                return emptyCtx;
            }
        }
    );

    // 使用 QFutureWatcher 监听结果（在主线程回调）
    auto* watcher = new QFutureWatcher<PipelineContext>(this);
    connect(watcher, &QFutureWatcher<PipelineContext>::finished, this,
        [this, watcher, idx, imageId, imageName]() {
            PipelineContext ctx = watcher->result();
            watcher->deleteLater();

            if (idx >= m_results.size()) return;

            BatchDetectionItem& item = m_results[idx];
            item.processed = true;
            item.passed = ctx.pass;
            item.statusText = ctx.pass ? "PASS" : QString("FAIL: %1").arg(ctx.reason);

            // 生成报告
            DetectionResultReport report = DetectionResultReport::fromPipelineContext(
                ctx, imageId, QString(), QString());
            item.roiReports.append(report);

            // 更新表格
            int row = m_ui->tableWidget_results->rowCount();
            m_ui->tableWidget_results->insertRow(row);
            m_ui->tableWidget_results->setItem(row, 0,
                new QTableWidgetItem(QString::number(idx + 1)));
            m_ui->tableWidget_results->setItem(row, 1,
                new QTableWidgetItem(imageName));
            m_ui->tableWidget_results->setItem(row, 2,
                new QTableWidgetItem(ctx.pass ? "✅ PASS" : "❌ FAIL"));
            m_ui->tableWidget_results->setItem(row, 3,
                new QTableWidgetItem(ctx.pass ? "正常" : ctx.reason));
            m_ui->tableWidget_results->setItem(row, 4,
                new QTableWidgetItem(QString::number(m_elapsedTimer.elapsed())));

            // 颜色标记
            QColor bgColor = ctx.pass ? QColor(220, 255, 220) : QColor(255, 220, 220);
            for (int c = 0; c < 5; ++c) {
                m_ui->tableWidget_results->item(row, c)->setBackground(bgColor);
            }

            // 更新统计
            int passCount = 0, failCount = 0;
            for (const auto& r : m_results) {
                if (!r.processed) continue;
                if (r.passed) passCount++;
                else failCount++;
            }

            m_ui->label_summary->setText(
                QString("统计: 总计 %1 | 已完成 %2 | ✅ 通过 %3 | ❌ 失败 %4 | 通过率: %5%")
                    .arg(m_results.size())
                    .arg(idx + 1)
                    .arg(passCount)
                    .arg(failCount)
                    .arg(passCount * 100.0 / (idx + 1), 0, 'f', 1));

            emit imageResultReady(idx, imageName, ctx.pass);
            emit logMessage(QString("[%1/%2] %3: %4")
                .arg(idx + 1).arg(m_results.size()).arg(imageName)
                .arg(ctx.pass ? "PASS" : "FAIL"));

            // 处理下一张
            m_currentIndex = idx + 1;
            processNextImage();
        }
    );
    watcher->setFuture(future);
}

void BatchDetectionWidget::finishDetection()
{
    m_running = false;
    m_paused = false;

    int passCount = 0, failCount = 0;
    for (const auto& r : m_results) {
        if (r.passed) passCount++;
        else failCount++;
    }

    m_ui->progressBar->setValue(100);
    m_ui->label_status->setText("检测完成");
    m_ui->label_summary->setText(
        QString("检测完成: 总计 %1 | ✅ 通过 %2 | ❌ 失败 %3 | 通过率: %4% | 耗时: %5s")
            .arg(m_results.size())
            .arg(passCount)
            .arg(failCount)
            .arg(passCount * 100.0 / qMax(1, m_results.size()), 0, 'f', 1)
            .arg(m_elapsedTimer.elapsed() / 1000.0, 0, 'f', 1));

    m_ui->btn_exportCsv->setEnabled(m_results.size() > 0);

    updateUiState(false);

    emit detectionFinished(passCount, failCount);
    emit logMessage(QString("批量检测完成: %1张图片, 通过%2, 失败%3")
        .arg(m_results.size()).arg(passCount).arg(failCount));
}

void BatchDetectionWidget::updateUiState(bool running)
{
    m_ui->btn_start->setEnabled(!running);
    m_ui->btn_pause->setEnabled(running);
    m_ui->btn_stop->setEnabled(running);
    m_ui->comboBox_profile->setEnabled(!running);
    m_ui->btn_addImageFolder->setEnabled(!running);

    if (!running) {
        m_ui->btn_pause->setText("⏸ 暂停");
    }
}

// ==================== 导出 ====================

void BatchDetectionWidget::onExportCsvClicked()
{
    if (m_results.isEmpty()) return;

    QString filePath = QFileDialog::getSaveFileName(
        this, "导出检测报告", "detection_report.csv",
        "CSV文件 (*.csv);;所有文件 (*)");

    if (filePath.isEmpty()) return;

    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::warning(this, "导出失败", "无法创建文件");
        return;
    }

    QTextStream out(&file);
    out.setEncoding(QStringConverter::Utf8);

    // BOM for Excel
    out << "\xEF\xBB\xBF";

    // 标题行
    out << "序号,图片名称,检测结果,详情,时间戳\n";

    // 数据行
    for (int i = 0; i < m_results.size(); ++i) {
        const BatchDetectionItem& item = m_results[i];
        out << QString::number(i + 1) << ","
            << item.imageName << ","
            << (item.passed ? "PASS" : "FAIL") << ","
            << item.statusText << ","
            << QDateTime::currentDateTime().toString(Qt::ISODate) << "\n";
    }

    file.close();

    QMessageBox::information(this, "导出成功",
        QString("报告已导出到: %1").arg(filePath));
    emit logMessage(QString("检测报告已导出: %1").arg(filePath));
}