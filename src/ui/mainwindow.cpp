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

// Tab Widget头文件
#include "widgets/video_tab_widget.h"

#include <QFile>
#include <QDir>
#include <QTimer>
#include <QFileDialog>

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
}

void MainWindow::setupConnections()
{
    connect(ui->tabWidget, &QTabWidget::currentChanged, this, &MainWindow::on_tabWidget_currentChanged);

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

    m_signalConnector = new SignalConnector(this, m_pipelineManager, &m_roiManager,
                                           m_view, m_tabManager, m_roiUiController,
                                           m_detectionUiController, this);
    connect(m_tabManager, &TabManager::tabCreated,
            m_signalConnector, &SignalConnector::connectTabSignals);

    m_imageListManager = new ImageListManager(m_roiManager, m_fileManager, this, this);
    m_imageListManager->init(ui->listWidget_images, ui->btn_addImage, ui->btn_removeImage);
    connect(m_imageListManager, &ImageListManager::imageDisplayRequested, this, &MainWindow::showImage);
    connect(m_imageListManager, &ImageListManager::processRequested, this, &MainWindow::processAndDisplay);

    connect(&m_roiManager, &RoiManager::currentImageChanged, this, [this](const QString&) {
        m_roiUiController->syncRoiConfigsToWidget();
    });
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

    // 全局信号连接
    connect(m_roiUiController, &RoiUiController::roiChanged, this, &MainWindow::processAndDisplay);
    connect(m_configController, &ConfigController::configApplied, this, &MainWindow::processAndDisplay);

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

// ========== ROI操作 ==========

void MainWindow::on_btn_drawRoi_clicked()
{
    m_roiManager.enableDrawRoiMode(m_view, ui->statusbar);
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
    processAndDisplay();
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
        m_tabManager->getProcessTab()
    );
}