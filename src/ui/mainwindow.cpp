#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "logger.h"
#include "config/constants.h"
#include "image_view.h"
#include "signal_connector.h"
#include <QtConcurrent/QtConcurrent>

// 各模块完整定义
#include "system_monitor.h"
#include "file_manager.h"
#include "config_manager.h"
#include "widgets/image_list_manager.h"
#include "widgets/tab_manager.h"
#include "roi_config.h"
#include "log_page.h"
#include "controllers/roi_ui_controller.h"
#include "controllers/detection_ui_controller.h"
#include "controllers/config_controller.h"
#include "controllers/auto_detection_controller.h"
#include "ui/pipeline_result_handler.h"
#include "core/profile_manager.h"
#include "ui/cloud_dashboard_manager.h"

// Tab Widget头文件
#include "widgets/video_tab_widget.h"
#include "widgets/barcode_tab_widget.h"
#include "widgets/template_tab_widget.h"

#include <QFile>
#include <QDir>
#include <QTimer>
#include <QFileDialog>
#include <QInputDialog>
#include <QLineEdit>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , m_pipelineManager(new PipelineManager(this))
    , m_systemMonitor(new SystemMonitor(this))
    , m_fileManager(new FileManager(this))
{
    ui->setupUi(this);
    setWindowState(Qt::WindowMaximized);
    showMaximized();

    setupBasicInfrastructure();
    setupManagers();
    setupControllers();
    setupResultHandler();
    setupMqtt();
}

MainWindow::~MainWindow()
{
    m_isDestroying = true;
    if (m_isProcessing) {
        m_pipelineWatcher.waitForFinished();
    }
    disconnect(&m_pipelineWatcher, nullptr, this, nullptr);
    disconnect(Logger::instance(), nullptr, this, nullptr);
    delete ui;
}

// ========== 初始化 ==========

void MainWindow::setupBasicInfrastructure()
{
    // 防抖定时器
    m_processDebounceTimer = new QTimer(this);
    m_processDebounceTimer->setSingleShot(true);
    m_processDebounceTimer->setInterval(AppConstants::DEBOUNCE_PROCESS_MS);
    connect(m_processDebounceTimer, &QTimer::timeout, this, &MainWindow::processAndDisplay);

    setupUI();
    setupConnections();

    // 系统监控
    m_systemMonitor->setupWithStatusBar(ui->statusbar, AppConstants::SYSTEM_MONITOR_INTERVAL_MS);

    // 日志初始化
    QString logDir = PROJECT_ROOT_DIR "/logs";
    QDir(logDir).mkpath(".");
    Logger::instance()->setLogFile(logDir + "/test.log");
    Logger::instance()->enableFileLog(true);

    ui->page_log->initialize();
    connect(ui->page_log, &LogPage::requestGoHome, this, [this]() {
        ui->stackedWidget_main->setCurrentIndex(0);
    });
}

void MainWindow::setupUI()
{
    m_view = static_cast<ImageView*>(ui->graphicsView);
    QFile file(":/style.qss");
    if (file.open(QFile::ReadOnly)) {
        qApp->setStyleSheet(QLatin1String(file.readAll()));
    }

    // Toast 浮标（叠加在图像视图上）
    m_toast = new ToastNotification(centralWidget());
}

void MainWindow::setupConnections()
{
    // 注意：on_tabWidget_currentChanged 已由 ui->setupUi 内部的 connectSlotsByName 自动连接，无需手动连接

    // FileManager信号
    connect(m_fileManager, &FileManager::imageLoaded, this, [this](const cv::Mat& img) {
        m_roiManager.setFullImage(img);
        m_view->clearRoi();
        m_pipelineManager->resetPipeline();
        showImage(img);
        Logger::instance()->info("图像加载成功!");
    });
    connect(m_fileManager, &FileManager::imageSaved, this, [](const QString& path) {
        Logger::instance()->info(QString("图像保存成功: %1").arg(path));
    });
    connect(m_fileManager, &FileManager::errorOccurred, this, [this](const QString& message) {
        Logger::instance()->error(message);
        QMessageBox::warning(this, "错误", message);
    });

    // ImageView信号
    connect(m_view, &ImageView::pixelInfoChanged, this, [this](int x, int y, const QColor &color, int gray) {
        ui->statusbar->showMessage(
            QString("X=%1 Y=%2  R=%3 G=%4 B=%5  Gray=%6").arg(x).arg(y)
                .arg(color.red()).arg(color.green()).arg(color.blue()).arg(gray));
        QTimer::singleShot(AppConstants::PIXEL_INFO_TIMEOUT_MS, ui->statusbar, &QStatusBar::clearMessage);
    });
    connect(m_view, &ImageView::polygonDrawingPointAdded, this,
        [](const QString& type, const QPointF& point) {
            Logger::instance()->info(QString("[%1] 添加顶点: (%2, %3)")
                .arg(type).arg(point.x(), 0, 'f', 1).arg(point.y(), 0, 'f', 1));
        });
}

void MainWindow::setupManagers()
{
    m_tabManager = new TabManager(ui->tabWidget, m_pipelineManager, &m_roiManager,
                                   m_view, m_processDebounceTimer, this);

    // 创建云平台看板管理器
    m_cloudDashboardManager = new CloudDashboardManager(m_toast, this);

    // 创建检测方案管理器
    m_profileManager = new ProfileManager(&m_roiManager, m_pipelineManager, this);

    m_imageListManager = new ImageListManager(m_roiManager, m_fileManager, this, this);
    m_imageListManager->init(ui->listWidget_images, ui->btn_addImage, ui->btn_removeImage);
    connect(m_imageListManager, &ImageListManager::imageDisplayRequested, this, &MainWindow::showImage);
    connect(m_imageListManager, &ImageListManager::processRequested, this, [this]() { requestRefresh(); });

}

void MainWindow::setupControllers()
{
    m_roiUiController = new RoiUiController(m_roiManager, m_pipelineManager, m_view, ui->statusbar, this);
    m_detectionUiController = new DetectionUiController(m_roiManager, ui->tabWidget, this);
    m_configController = new ConfigController(m_pipelineManager, m_roiManager, this);
    m_autoDetectionController = new AutoDetectionController(m_pipelineManager, &m_roiManager, this);

    m_autoDetectionController->setupUiConnections(
        ui->btn_startAutoDetection, ui->btn_stopAutoDetection,
        ui->statusbar, ui->label_imageList);
    m_roiUiController->setupTreeView(ui->treeView);
    m_roiUiController->setupMainWindowConnections(m_tabManager);
    m_detectionUiController->setupConnections(m_roiUiController,
        [this](const QString& tabName) { ensureTabExists(tabName); });

    // 全局信号连接（统一走防抖 + 脏标记）
    connect(m_roiUiController, &RoiUiController::roiChanged, this, [this]() { requestRefresh(); });
    connect(m_configController, &ConfigController::configApplied, this, [this]() { requestRefresh(); });

    // 【修复】SignalConnector 必须在 controller 创建之后才能创建（需要有效的 roiUiController 指针）
    m_signalConnector = new SignalConnector(m_pipelineManager, &m_roiManager,
                                           m_view, m_roiUiController, this);
    connect(m_tabManager, &TabManager::tabCreated,
            m_signalConnector, &SignalConnector::connectTabSignals);
    // 连接 SignalConnector 的信号到 MainWindow 的方法
    connect(m_signalConnector, &SignalConnector::requestRefresh, this, &MainWindow::requestRefresh);
    connect(m_signalConnector, &SignalConnector::processAndDisplay, this, &MainWindow::processAndDisplay);
    connect(m_signalConnector, &SignalConnector::showImage, this, &MainWindow::showImage);

    // Tab懒加载时，更新ConfigController的tab指针
    connect(m_tabManager, &TabManager::tabCreated, this, [this](const QString& name, QWidget*) {
        m_configController->setTabWidgets(
            m_tabManager->getEnhanceTab(),
            m_tabManager->getFilterTab(),
            m_tabManager->getExtractTab(),
            m_tabManager->getJudgeTab(),
            m_tabManager->getProcessTab(),
            m_tabManager->getBarcodeTab()
        );
        // 新创建的模板tab需要注入ProfileManager
        if (name == "补正") {
            auto* templateTab = m_tabManager->getTemplateTab();
            if (templateTab) {
                templateTab->setProfileManager(m_profileManager);
            }
        }
        // 新创建的条码tab需要从PipelineManager同步配置到UI
        if (name == "条码") {
            auto* barcodeTab = m_tabManager->getBarcodeTab();
            if (barcodeTab) {
                PipelineConfig pc = m_pipelineManager->getConfigSnapshot();
                barcodeTab->setBarcodeConfig(pc.barcode);
            }
        }
    });

    // 批量检测完成提示
    connect(m_autoDetectionController, &AutoDetectionController::detectionFinished,
            this, [this](const DetectionStats&) {
        m_toast->showMessage("批量检测完成");
    });

    // 检测项选中跳转
    connect(m_roiUiController, &RoiUiController::detectionItemSelected, this,
        [this](const QString& roiId, const QString& detectionId) {
            m_detectionUiController->onDetectionItemSelected(roiId, detectionId, m_tabManager, m_pipelineManager);
        });

    // 工具栏按钮
    connect(ui->btn_addDetection, &QPushButton::clicked, this, [this]() {
        m_detectionUiController->onAddDetectionClicked(m_roiUiController->getCurrentSelectedRoiId());
    });
    connect(ui->btn_deleteDetection, &QPushButton::clicked, this, [this]() {
        QTreeWidgetItem* currentItem = ui->treeView->currentItem();
        if (!currentItem) {
            QMessageBox::warning(this, "警告", "请先选择要删除的检测项");
            return;
        }
        if (currentItem->data(0, Qt::UserRole + 1).toString() == "detection") {
            QTreeWidgetItem* parentItem = currentItem->parent();
            if (parentItem) {
                m_detectionUiController->onDeleteDetectionClicked(
                    parentItem->data(0, Qt::UserRole).toString(),
                    currentItem->data(0, Qt::UserRole).toString());
            }
        } else {
            QMessageBox::warning(this, "警告", "请先选择要删除的检测项");
        }
    });

    // roiManager → roiUiController 同步连接（必须在所有 controller 创建完成后）
    connect(&m_roiManager, &RoiManager::currentImageChanged, this, [this](const QString&) {
        m_roiUiController->syncRoiConfigsToWidget();
    });
}

void MainWindow::setupResultHandler()
{
    m_pipelineResultHandler = new PipelineResultHandler(this);
    m_pipelineResultHandler->setDependencies(m_tabManager, &m_roiManager, m_view, m_pipelineManager);
    m_pipelineResultHandler->watchPipeline(&m_pipelineWatcher);

    connect(m_pipelineResultHandler, &PipelineResultHandler::statusMessage,
            ui->statusbar, &QStatusBar::showMessage);
    connect(m_pipelineResultHandler, &PipelineResultHandler::processingFinished,
            this, [this]() {
        m_isProcessing = false;
        m_toast->showMessage("检测完成");
        if (m_hasPendingProcess) {
            m_hasPendingProcess = false;
            QTimer::singleShot(AppConstants::PENDING_PROCESS_DELAY_MS, this, &MainWindow::processAndDisplay);
        }
    });
}

// ========== 核心处理 ==========

void MainWindow::processAndDisplay()
{
    if (m_isProcessing) {
        m_hasPendingProcess = true;
        return;
    }

    // 脏标记检查：没有变化时跳过Pipeline
    if (!m_needRefresh) return;
    m_needRefresh = false;

    setDisplayModeForCurrentTab();

    cv::Mat currentImage = m_roiManager.getCurrentImage();
    if (currentImage.empty()) return;

    m_isProcessing = true;
    m_hasPendingProcess = false;
    ui->statusbar->showMessage("正在处理...");

    PipelineConfig configSnapshot = m_pipelineManager->getConfigSnapshot();
    QFuture<PipelineContext> future = QtConcurrent::run(
        [this, currentImage, configSnapshot]() -> PipelineContext {
            return m_pipelineManager->execute(currentImage, configSnapshot);
        }
    );
    m_pipelineWatcher.setFuture(future);
}

// ========== Tab名称 → 显示模式 ==========

static const QHash<QString, DisplayConfig::Mode> s_tabDisplayMode = {
    {"图像",     DisplayConfig::Mode::Channel},
    {"视频",     DisplayConfig::Mode::Channel},
    {"增强",     DisplayConfig::Mode::Enhanced},
    {"补正",     DisplayConfig::Mode::Original},
    {"处理",     DisplayConfig::Mode::Processed},
    {"提取",     DisplayConfig::Mode::Processed},
    {"判定",     DisplayConfig::Mode::MaskOverlay},
    {"直线",     DisplayConfig::Mode::LineDetect},
    {"条码",     DisplayConfig::Mode::Original},
    {"目标检测", DisplayConfig::Mode::Original},
};

void MainWindow::setDisplayModeForCurrentTab()
{
    int idx = ui->tabWidget->currentIndex();
    if (idx < 0) return;
    QString tabName = ui->tabWidget->tabText(idx);

    DisplayConfig::Mode mode = s_tabDisplayMode.value(tabName, DisplayConfig::Mode::Original);

    if (tabName == "过滤") {
        PipelineConfig cfg = m_pipelineManager->getConfigSnapshot();
        mode = (cfg.currentFilterMode == PipelineConfig::FilterMode::None)
            ? DisplayConfig::Mode::Original
            : DisplayConfig::Mode::MaskGreenWhite;
    }

    m_pipelineManager->setDisplayMode(mode);
}

void MainWindow::showImage(const cv::Mat &img)
{
    m_view->setImage(ImageUtils::matToQImage(img));
}

// ========== 文件操作 ==========

void MainWindow::on_btn_openImg_clicked()
{
    QString path = QCoreApplication::applicationDirPath() + "/../../images/";
    if (!QDir(path).exists()) {
        path = QCoreApplication::applicationDirPath() + "/images/";
    }

    QString fileName = m_fileManager->selectImageFile(path);
    if (!fileName.isEmpty()) {
        m_fileManager->readImageFile(fileName);
    } else {
        Logger::instance()->warning("用户取消了文件选择");
    }
}

void MainWindow::on_btn_openVideo_clicked()
{
    auto* videoTab = m_tabManager->getVideoTab();
    if (!videoTab) {
        Logger::instance()->error("视频Tab组件不存在");
        return;
    }

    VideoManager* videoManager = videoTab->getVideoManager();
    if (!videoManager) {
        Logger::instance()->error("无法获取视频管理器");
        return;
    }

    QString filePath = QFileDialog::getOpenFileName(
        this, "选择视频文件", "",
        "视频文件 (*.mp4 *.avi *.mov *.mkv *.wmv);;所有文件 (*.*)");

    if (filePath.isEmpty()) {
        Logger::instance()->info("用户取消了文件选择");
        return;
    }

    if (videoManager->openFile(filePath)) {
        int videoTabIndex = ui->tabWidget->indexOf(videoTab);
        if (videoTabIndex >= 0) {
            ui->tabWidget->setCurrentIndex(videoTabIndex);
        }
        Logger::instance()->info(QString("打开视频成功: %1").arg(filePath));
    } else {
        Logger::instance()->error(QString("视频管理器打开文件失败: %1").arg(filePath));
    }
}

void MainWindow::on_btn_saveImg_clicked()
{
    m_fileManager->saveImageFileWithDialog(m_pipelineManager);
}

void MainWindow::on_btn_cloudPlatform_clicked()
{
    m_cloudDashboardManager->launch();
}

// ========== ROI操作 ==========

void MainWindow::on_btn_drawRoi_clicked()
{
    if (!m_roiManager.getFullImage().empty()) {
        m_view->setRoiMode(true);
        m_view->setDragMode(QGraphicsView::NoDrag);
        ui->statusbar->showMessage("请按下左键绘制ROI");
    }
}

void MainWindow::on_btn_resetROI_clicked()    { m_roiUiController->onResetRoiClicked(); }
void MainWindow::onRoiSelected(const QRectF &r) { m_roiUiController->handleRoiSelectedComplete(r); }
void MainWindow::on_btn_addROI_clicked()      { m_roiUiController->onAddRoiClicked(); }
void MainWindow::on_btn_deleteROI_clicked()   { m_roiUiController->onDeleteRoiClicked(); }
void MainWindow::on_btn_switchROI_clicked()   { m_roiUiController->onSwitchRoiClicked(); }

// ========== Tab切换 ==========

void MainWindow::on_tabWidget_currentChanged(int index)
{
    if (m_isDestroying) return;
    if (m_roiManager.getFullImage().empty()) return;

    // 只有显示模式变化时才需要刷新显示
    DisplayConfig::Mode newMode = getDisplayModeForTab(index);
    if (newMode != m_lastDisplayMode) {
        m_lastDisplayMode = newMode;
        // 优先使用快速渲染（不重新执行Pipeline），如果没有上次结果才触发Pipeline
        displayCurrentResult();
    }
}

// ========== 页面导航 ==========

void MainWindow::on_btn_Log_clicked()
{
    ui->stackedWidget_main->setCurrentIndex(1);
}

void MainWindow::on_btn_Home_clicked()
{
    ui->stackedWidget_main->setCurrentIndex(0);
}

// ========== 配置管理 ==========

void MainWindow::on_btn_saveConfig_clicked()  { m_configController->saveConfig(this); }
void MainWindow::on_btn_importConfig_clicked() { m_configController->loadConfig(this); }

// ========== 自动检测 ==========

void MainWindow::on_btn_startAutoDetection_clicked()
{
    m_roiUiController->saveCurrentRoiPipelineConfig();
    m_autoDetectionController->startDetection();
}

void MainWindow::on_btn_stopAutoDetection_clicked()
{
    m_autoDetectionController->stopDetection();
}

// ========== MQTT 边云协同 ==========

void MainWindow::setupMqtt()
{
    // 加载 MQTT 配置（优先从默认配置文件读取）
    AppConfig appConfig;
    MqttConfig mqttCfg;
    if (ConfigManager::instance().loadConfig(appConfig, ConfigManager::instance().getDefaultConfigPath())) {
        mqttCfg = appConfig.mqttConfig;
    }

    m_mqttManager = new MqttManager(this);
    m_mqttManager->initialize(mqttCfg);

    // Feature 1: 自动检测逐张完成 → 上报检测结果
    connect(m_autoDetectionController, &AutoDetectionController::imageProcessed,
            this, [this](int index, const QString&, bool) {
        const auto& results = m_autoDetectionController->results();
        if (index >= 0 && index < results.size()) {
            m_mqttManager->publishResult(results[index]);
        }
    });

    // Feature 2: 云端指令 → 调用本地功能
    connect(m_mqttManager, &MqttManager::captureRequested,
            this, [this]() {
        m_toast->showMessage("收到云端指令: capture");
        Logger::instance()->info("[MQTT] 收到capture指令，执行采集检测");
        requestRefresh();
    });
    connect(m_mqttManager, &MqttManager::startDetectionRequested,
            this, [this]() {
        m_toast->showMessage("收到云端指令: 开始批量检测");
        on_btn_startAutoDetection_clicked();
    });
    connect(m_mqttManager, &MqttManager::stopDetectionRequested,
            this, [this]() {
        m_toast->showMessage("收到云端指令: 停止批量检测");
        on_btn_stopAutoDetection_clicked();
    });

    // Feature 3: 心跳由 MqttManager 内部定时器驱动，连接成功即自动开始

    // 连接状态日志
    connect(m_mqttManager, &MqttManager::mqttConnected, this, []() {
        Logger::instance()->info("[MQTT] 已连接到云端");
    });
    connect(m_mqttManager, &MqttManager::mqttDisconnected, this, []() {
        Logger::instance()->info("[MQTT] 已断开连接");
    });
    connect(m_mqttManager, &MqttManager::mqttError, this, [](const QString& msg) {
        Logger::instance()->error(QString("[MQTT] %1").arg(msg));
    });

    Logger::instance()->info(QString("[MQTT] 初始化完成, enabled=%1, broker=%2:%3")
        .arg(mqttCfg.enabled ? "true" : "false")
        .arg(mqttCfg.brokerHost)
        .arg(mqttCfg.brokerPort));

    // 云平台按钮由 Qt 自动连接 (on_btn_cloudPlatform_clicked)
}

// ========== Tab懒加载 ==========

void MainWindow::ensureTabExists(const QString& tabName)
{
    if (m_tabManager->isCreated(tabName)) return;

    m_tabManager->ensureTab(tabName);

    m_detectionUiController->setTabNames(m_tabManager->createdTabNames());
    m_configController->setTabWidgets(
        m_tabManager->getEnhanceTab(),
        m_tabManager->getFilterTab(),
        m_tabManager->getExtractTab(),
        m_tabManager->getJudgeTab(),
        m_tabManager->getProcessTab(),
        m_tabManager->getBarcodeTab()
    );
}

// ========== 刷新请求 ==========

void MainWindow::requestRefresh()
{
    m_needRefresh = true;
    m_processDebounceTimer->start();
}

DisplayConfig::Mode MainWindow::getDisplayModeForTab(int index) const
{
    if (index < 0 || index >= ui->tabWidget->count()) {
        return DisplayConfig::Mode::Original;
    }
    QString tabName = ui->tabWidget->tabText(index);
    return s_tabDisplayMode.value(tabName, DisplayConfig::Mode::Original);
}

void MainWindow::displayCurrentResult()
{
    if (!m_pipelineManager->hasLastResult()) {
        // 没有上次Pipeline结果，需要触发完整处理
        requestRefresh();
        return;
    }
    DisplayConfig::Mode mode = getDisplayModeForTab(ui->tabWidget->currentIndex());
    cv::Mat displayImage = m_pipelineManager->getLastDisplayWithMode(mode);
    if (!displayImage.empty()) {
        m_view->setImage(ImageUtils::matToQImage(displayImage));
    }
}

// ========== 方案操作 ==========

void MainWindow::on_btn_saveToProfile_clicked()
{
    if (!m_profileManager) return;

    cv::Mat currentImage = m_roiManager.getFullImage();
    if (currentImage.empty()) {
        QMessageBox::warning(this, "提示", "请先打开图片");
        return;
    }

    bool ok;
    QString name = QInputDialog::getText(this, "保存检测方案", "方案名称:",
                                          QLineEdit::Normal, QString(), &ok);
    if (!ok || name.trimmed().isEmpty()) return;

    if (m_profileManager->saveCurrentAsProfile(name.trimmed())) {
        m_toast->showMessage(QString("方案 '%1' 已保存").arg(name.trimmed()));
        Logger::instance()->info(QString("[MainWindow] 方案已保存: %1").arg(name.trimmed()));
    } else {
        QMessageBox::warning(this, "保存失败", "保存方案失败，请查看日志");
    }
}

void MainWindow::on_btn_loadFromProfile_clicked()
{
    if (!m_profileManager) return;

    QList<ProfileManager::ProfileSummary> profiles = m_profileManager->getProfileList();
    if (profiles.isEmpty()) {
        QMessageBox::information(this, "提示", "没有已保存的检测方案\n请先点击「保存到方案」创建方案");
        return;
    }

    QStringList names;
    QList<QString> ids;
    for (const auto& p : profiles) {
        names.append(QString("%1 (ROI:%2, 模板:%3)").arg(p.profileName).arg(p.roiCount).arg(p.templateCount));
        ids.append(p.profileId);
    }

    bool ok = false;
    QString selected = QInputDialog::getItem(this, "加载检测方案", "选择方案:", names, 0, false, &ok);
    if (!ok) return;

    int idx = names.indexOf(selected);
    if (idx < 0) return;

    if (m_profileManager->loadProfile(ids[idx])) {
        m_roiUiController->syncRoiConfigsToWidget();

        // 从已加载的ROI中同步条码配置到全局和条码Tab UI
        QList<RoiConfig> rois = m_roiManager.getRoiConfigs();
        for (const auto& roi : rois) {
            bool hasBarcode = false;
            for (const auto& det : roi.detectionItems) {
                if (det.type == DetectionType::Barcode && det.enabled) {
                    hasBarcode = true;
                    break;
                }
            }
            if (hasBarcode) {
                PipelineConfig pc = m_pipelineManager->getConfigSnapshot();
                pc.barcode = roi.pipelineConfig.barcode;
                m_pipelineManager->setConfig(pc);
                auto* barcodeTab = m_tabManager->getBarcodeTab();
                if (barcodeTab) {
                    barcodeTab->setBarcodeConfig(pc.barcode);
                }
                break;
            }
        }

        m_toast->showMessage(QString("方案 '%1' 已加载").arg(profiles[idx].profileName));
        requestRefresh();
    } else {
        QMessageBox::warning(this, "加载失败", "加载方案失败，请确保已打开图片");
    }
}

void MainWindow::on_btn_importFolder_clicked()
{
    QString dir = QFileDialog::getExistingDirectory(this, "选择图片文件夹", QString(),
        QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);
    if (dir.isEmpty()) return;

    QApplication::setOverrideCursor(Qt::WaitCursor);
    QStringList imported = m_roiManager.importImagesFromFolder(dir);
    QApplication::restoreOverrideCursor();

    if (imported.isEmpty()) {
        QMessageBox::information(this, "导入结果", "未导入任何图片。\n请确保文件夹中含有支持格式: .jpg, .png, .bmp, .tiff");
    } else {
        m_toast->showMessage(QString("已导入 %1 张图片").arg(imported.size()));
        Logger::instance()->info(QString("[MainWindow] 从文件夹导入 %1 张图片: %2").arg(imported.size()).arg(dir));
    }
}
