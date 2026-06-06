#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "logger.h"
#include "config/constants.h"
#include "image_view.h"
#include "widgets/i_tab_interfaces.h"
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
#include "controllers/roi_detection_config_controller.h"
#include "controllers/detection_ui_controller.h"
#include "controllers/config_controller.h"
#include "controllers/auto_detection_controller.h"
#include "controllers/pipeline_result_handler.h"
#include "core/profile_manager.h"
#include "ui/cloud_dashboard_manager.h"
#include "ui/display_mode_manager.h"
#include "controllers/profile_controller.h"

// Tab Widget头文件
#include "widgets/video_tab_widget.h"
#include "widgets/barcode_tab_widget.h"
#include "widgets/template_tab_widget.h"
#include "widgets/step_config_widget.h"


#include <QTabBar>
#include <QFile>
#include <QThread>
#include <QDir>
#include <QTimer>
#include <QFileDialog>
#include <QInputDialog>
#include <QLineEdit>
#include <QGroupBox>


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
    setupControllerConnections();
    setupTabRegistration();
    setupResultHandler();
    setupMqtt();
}

MainWindow::~MainWindow()
{
    stopAllAsyncOperations();
    delete ui;
}

// ========== 异步资源清理 ==========

void MainWindow::stopAllAsyncOperations()
{
    m_isDestroying = true;

    // 停止视频播放并断开信号，防止定时器回调访问已销毁的成员
    if (m_tabManager) {
        VideoTabWidget* videoTab = m_tabManager->getTabAs<VideoTabWidget>("视频");
        if (videoTab) {
            VideoManager* vm = videoTab->getVideoManager();
            if (vm) {
                vm->stop();
                videoTab->disconnect();
                vm->disconnect();
            }
        }
    }

    // 取消所有待处理的Pipeline请求
    m_pipelineManager->scheduler()->cancelAll();

    disconnect(Logger::instance(), nullptr, this, nullptr);
}

// ========== 初始化 ==========

void MainWindow::setupBasicInfrastructure()
{
    setupUI();
    setupAnimations();
    setupConnections();

    // 系统监控
    m_systemMonitor->setupWithStatusBar(ui->statusbar, AppConstants::SYSTEM_MONITOR_INTERVAL_MS);

    // Pipeline 耗时标签（StatusBar 永久显示，最左侧）
    m_timingLabel = new QLabel(this);
    m_timingLabel->setTextFormat(Qt::RichText);
    m_timingLabel->setText("<img src=':/icons/pipeline.png' width='16' height='16' style='vertical-align: middle;'> Pipeline: -- ms");
    ui->statusbar->insertPermanentWidget(0, m_timingLabel);
    m_benchmarkUiTimer = new QTimer(this);
    m_benchmarkUiTimer->setInterval(500);
    connect(m_benchmarkUiTimer, &QTimer::timeout, this, [this]() {
        // 从调度器获取最新的执行时间
        if (m_pipelineManager->scheduler()->isProcessing()) {
            return;
        }
        // 使用 lastExecMs 获取上次执行时间
        double ms = m_pipelineManager->lastExecMs();
        if (ms > 0) {
            m_timingLabel->setText(QString("Pipeline: %1 ms").arg(ms, 0, 'f', 1));
        }
    });
    m_benchmarkUiTimer->start();

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
    m_toast = new ToastNotification(m_view);
}

void MainWindow::setupConnections()
{
    // 注意：on_tabWidget_currentChanged 已由 ui->setupUi 内部的 connectSlotsByName 自动连接，无需手动连接

    // FileManager信号
    connect(m_fileManager, &FileManager::imageLoaded, this, [this](const cv::Mat& img, const QString& filePath) {
        m_roiManager.setFullImage(img, filePath);
        m_view->clearRoi();
        
        // 清空Pipeline结果，防止旧结果污染新图片显示
        m_pipelineManager->clearLastResult();
        m_pipelineManager->resetPipeline();
        
        // [FIX] 加载新图片的Pipeline配置（在resetPipeline之后）
        QString imageId = m_roiManager.getCurrentImageId();
        if (!imageId.isEmpty()) {
            PipelineConfig imageConfig = m_roiManager.loadImagePipelineConfig(imageId);
            m_pipelineManager->setConfig(imageConfig);
            m_pipelineManager->rebuildPipeline();
        }
        
        // 清空画布，显示新原图
        showImage(img);
        
        // 清空所有已创建Tab页的结果显示
        for (auto it = m_tabManager->allTabs().constBegin();
             it != m_tabManager->allTabs().constEnd(); ++it) {
            if (auto* updatable = dynamic_cast<IResultUpdatable*>(it.value())) {
                PipelineContext emptyCtx;
                updatable->updateFromPipeline(emptyCtx);
            }
        }
        
        Logger::instance()->info("图像加载成功!");
        
        // 触发一次pipeline执行，更新所有Tab的显示
        // （新图片可能还没有执行过pipeline，需要执行一次来初始化显示）
        QTimer::singleShot(100, this, [this]() {
            requestRefresh();
        });
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

void MainWindow::setupAnimations()
{
    m_pageOpacityEffect = new QGraphicsOpacityEffect(this);
    m_pageOpacityEffect->setOpacity(1.0);
    ui->page_home->setGraphicsEffect(m_pageOpacityEffect);

    m_pageFadeAnimation = new QPropertyAnimation(m_pageOpacityEffect, "opacity", this);
    m_pageFadeAnimation->setDuration(280);
    m_pageFadeAnimation->setStartValue(0.0);
    m_pageFadeAnimation->setEndValue(1.0);
    m_pageFadeAnimation->setEasingCurve(QEasingCurve::OutCubic);
}

void MainWindow::fadeIn(QWidget* widget, int durationMs)
{
    auto* effect = new QGraphicsOpacityEffect(widget);
    effect->setOpacity(0.0);
    widget->setGraphicsEffect(effect);

    auto* anim = new QPropertyAnimation(effect, "opacity", widget);
    anim->setDuration(durationMs);
    anim->setStartValue(0.0);
    anim->setEndValue(1.0);
    anim->setEasingCurve(QEasingCurve::OutCubic);
    anim->start(QAbstractAnimation::DeleteWhenStopped);
}

void MainWindow::fadeOut(QWidget* widget, int durationMs)
{
    auto* effect = qobject_cast<QGraphicsOpacityEffect*>(widget->graphicsEffect());
    if (!effect) {
        effect = new QGraphicsOpacityEffect(widget);
        widget->setGraphicsEffect(effect);
    }
    effect->setOpacity(1.0);

    auto* anim = new QPropertyAnimation(effect, "opacity", widget);
    anim->setDuration(durationMs);
    anim->setStartValue(1.0);
    anim->setEndValue(0.0);
    anim->setEasingCurve(QEasingCurve::InCubic);
    anim->start(QAbstractAnimation::DeleteWhenStopped);
}

void MainWindow::animatePageSwitch(int fromIndex, int toIndex)
{
    if (fromIndex == toIndex) return;

    QWidget* fromPage = ui->stackedWidget_main->widget(fromIndex);
    QWidget* toPage = ui->stackedWidget_main->widget(toIndex);

    if (!fromPage || !toPage) {
        ui->stackedWidget_main->setCurrentIndex(toIndex);
        return;
    }

    toPage->setGraphicsEffect(nullptr);
    ui->stackedWidget_main->setCurrentIndex(toIndex);
    fadeIn(toPage, 280);
}

void MainWindow::setupManagers()
{
    m_tabManager = new TabManager(ui->tabWidget, m_pipelineManager, &m_roiManager,
                                   m_view, statusBar(), this);

    // 创建云平台看板管理器
    m_cloudDashboardManager = new CloudDashboardManager(m_toast, this);

    // 创建显示模式管理器
    m_displayModeManager = new DisplayModeManager(ui->tabWidget, m_pipelineManager, m_view, this);
    connect(m_displayModeManager, &DisplayModeManager::requestRefresh, this, &MainWindow::requestRefresh);

    // 创建检测方案管理器
    m_profileManager = new ProfileManager(&m_roiManager, m_pipelineManager, this);

    m_imageListManager = new ImageListManager(m_roiManager, m_fileManager, this, this);
    m_imageListManager->init(ui->listWidget_images, ui->btn_addImage, ui->btn_removeImage);
    connect(m_imageListManager, &ImageListManager::imageDisplayRequested, this, &MainWindow::showImage);
    connect(m_imageListManager, &ImageListManager::processRequested, this, [this]() { 
        m_pipelineMode = PipelineMode::Execute;
        requestRefresh(); 
    });
}

void MainWindow::setupControllers()
{
    // 创建控制器
    m_roiDetectionConfigController = new RoiDetectionConfigController(m_roiManager, this);
    m_roiUiController = new RoiUiController(m_roiManager, m_pipelineManager, m_view, ui->statusbar, this);
    m_roiUiController->setDetectionConfigController(m_roiDetectionConfigController);
    m_detectionUiController = new DetectionUiController(m_roiManager, ui->tabWidget, this);
    m_configController = new ConfigController(m_pipelineManager, m_roiManager, this);
    m_autoDetectionController = new AutoDetectionController(m_pipelineManager, &m_roiManager, this);
    m_autoDetectionController->setTabManager(m_tabManager);
    m_profileController = new ProfileController(m_profileManager, &m_roiManager,
                                                m_pipelineManager, m_tabManager,
                                                m_roiUiController, m_toast, this, this);

    // 初始化控制器 UI
    // 检测状态标签（创建并放置到状态栏）
    auto* detectionStatusLabel = new QLabel("");
    detectionStatusLabel->setObjectName("label_detectionStatus");
    detectionStatusLabel->setStyleSheet(
        "font-size: 11px; color: #6B7280; padding: 2px 4px;");
    ui->statusbar->insertPermanentWidget(1, detectionStatusLabel);
    m_autoDetectionController->setupUiConnections(
        ui->btn_startAutoDetection, ui->btn_stopAutoDetection,
        ui->statusbar, detectionStatusLabel);
    m_roiUiController->setupTreeView(ui->treeView);
    m_roiUiController->setupDisplayConnections(m_tabManager);
    m_detectionUiController->setupConnections(m_roiUiController,
        [this](const QString& tabName) { ensureTabExists(tabName); },
        [this]() {
            m_pipelineMode = PipelineMode::Execute;
            requestRefresh();
        });
}

void MainWindow::setupControllerConnections()
{
    // ROI变化：设置为执行模式并刷新
    connect(m_roiUiController, &RoiUiController::roiChanged, this, [this]() { 
        m_pipelineMode = PipelineMode::Execute;
        requestRefresh(); 
    });
    
    // 配置应用：设置为执行模式并刷新
    connect(m_configController, &ConfigController::configApplied, this, [this]() { 
        m_pipelineMode = PipelineMode::Execute;
        requestRefresh(); 
    });
    
    // 方案加载：设置为执行模式并刷新
    connect(m_profileController, &ProfileController::requestRefresh, this, [this]() {
        m_pipelineMode = PipelineMode::Execute;
        requestRefresh();
    });

    // Tab 信号连接（通过 ISignalConnectable 接口多态连接）
    connect(m_tabManager, &TabManager::tabCreated, this,
        [this](const QString& tabName, QWidget* widget) {
            auto* connectable = dynamic_cast<ISignalConnectable*>(widget);
            if (connectable) {
                connectable->connectSignals(
                    {m_pipelineManager, &m_roiManager, m_view, m_roiUiController},
                    [this]() {
                        // 用户主动请求执行：切换到执行模式
                        m_pipelineMode = PipelineMode::Execute;
                        requestRefresh();
                    }
                );
            } else {
                Logger::instance()->warning(
                    QString("[MainWindow] Tab '%1' 未实现 ISignalConnectable 接口，跳过信号连接").arg(tabName));
            }

            // 视频 Tab：播放状态切换时通知 PipelineResultHandler 切换推理后端
            if (auto* videoTab = qobject_cast<VideoTabWidget*>(widget)) {
                connect(videoTab->getVideoManager(), &VideoManager::playbackStateChanged,
                        this, [this](VideoManager::PlaybackState state) {
                            m_pipelineResultHandler->setVideoMode(state == VideoManager::PlaybackState::Playing);
                        });
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

            // 更新显示模式管理器的Tab状态（switchToTabConfig内部阻塞了信号，
            // 导致onTabChanged未触发，m_lastMode与实际Tab不同步）
            m_displayModeManager->onTabChanged(ui->tabWidget->currentIndex());

            if (auto* roi = m_roiManager.getRoiConfig(roiId)) {
                for (const auto& item : roi->detectionItems) {
                    if (item.itemId == detectionId) {
                        bool isVideoDetect = (item.type == DetectionType::VideoDetection);
                        m_view->setZoomEnabled(!isVideoDetect);
                        Logger::instance()->info(
                            QString("画布缩放: %1").arg(isVideoDetect ? "已禁用(视频检测模式)" : "已启用"));
                        break;
                    }
                }
            }
        });

    // [NOTE] 选中的ROI被删除时清空Tab结果
    connect(m_roiUiController, &RoiUiController::selectedRoiDeleted, this, [this]() {
        m_tabManager->clearAllResults();
    });

    // 工具栏按钮
    connect(ui->btn_addDetection, &QPushButton::clicked, this, [this]() {
        QString roiId = m_roiUiController->getCurrentSelectedRoiId();
        if (roiId.isEmpty()) {
            roiId = m_roiUiController->addFullImageRoi();
        }
        if (!roiId.isEmpty()) {
            m_detectionUiController->onAddDetectionClicked(roiId);
        }
    });
    connect(ui->btn_deleteDetection, &QPushButton::clicked, this, [this]() {
        m_detectionUiController->handleDeleteFromTree(ui->treeView);
    });

    // roiManager → roiUiController 同步连接
    // [FIX]：图片切换前保存旧图片的Pipeline配置（包括步骤组合）
    connect(&m_roiManager, &RoiManager::imageSwitching, this, [this](const QString& fromImageId, const QString&) {
        // 保存当前Pipeline配置到旧图片
        if (!fromImageId.isEmpty()) {
            m_roiManager.saveImagePipelineConfig(fromImageId, m_pipelineManager->getConfigSnapshot());
        }
        // 同时保存当前ROI的配置（如果有的话）
        m_roiUiController->saveCurrentRoiPipelineConfig();
    });
    // 图片切换后：更新Tab显示
    connect(&m_roiManager, &RoiManager::currentImageChanged, this, [this](const QString&) {
        m_pipelineManager->clearLastResult();
        m_roiUiController->syncRoiConfigsToWidget();
    });

    // 删除图片/ROI后：隐藏所有检测相关Tab
    connect(m_imageListManager, &ImageListManager::imageRemoved, this, &MainWindow::hideAllDetectionTabs);
    connect(m_roiUiController, &RoiUiController::tabVisibilityUpdateNeeded, this, &MainWindow::hideAllDetectionTabs);
    connect(m_detectionUiController, &DetectionUiController::tabVisibilityUpdateNeeded, this, &MainWindow::hideAllDetectionTabs);
}

void MainWindow::setupTabRegistration()
{
    // Tab 懒加载时，统一注册到 ConfigController + 初始化依赖注入
    connect(m_tabManager, &TabManager::tabCreated, this, [this](const QString& name, QWidget* widget) {
        Q_UNUSED(name);
        // 自动注册可配置 Tab
        if (auto* configurableTab = dynamic_cast<IConfigurableTab*>(widget)) {
            m_configController->registerTab(configurableTab);
        }
        // 自动执行 Tab 初始化（消除硬编码的名称判断）
        if (auto* initializableTab = dynamic_cast<ITabInitializable*>(widget)) {
            TabInitContext ctx;
            ctx.profileManager = m_profileManager;
            ctx.pipelineManager = m_pipelineManager;
            initializableTab->initializeTab(ctx);
        }

        // "步骤"Tab的 tabsNeeded 信号 → 按指定顺序创建/重排对应Tab
        if (auto* stepWidget = qobject_cast<StepConfigWidget*>(widget)) {
            connect(stepWidget, &StepConfigWidget::tabsNeeded, this, [this](const QStringList& tabNames) {
                // 1) 确保所有需要的 Tab 存在
                bool anyNew = false;
                for (const auto& name : tabNames) {
                    if (!m_tabManager->isCreated(name)) {
                        m_tabManager->ensureTab(name);
                        anyNew = true;
                    }
                }
                if (anyNew) {
                    m_detectionUiController->setTabNames(m_tabManager->createdTabNames());
                }

                // 2) 按指定顺序重排 TabBar
                auto* tabBar = ui->tabWidget->tabBar();
                for (int desired = 0; desired < tabNames.size(); ++desired) {
                    for (int cur = desired; cur < tabBar->count(); ++cur) {
                        if (tabBar->tabText(cur) == tabNames[desired]) {
                            if (cur != desired) {
                                tabBar->moveTab(cur, desired);
                            }
                            break;
                        }
                    }
                }
            });
        }
    });
}

void MainWindow::setupResultHandler()
{
    m_pipelineResultHandler = new PipelineResultHandler(this);
    m_pipelineResultHandler->setDependencies(m_tabManager, &m_roiManager, m_view, m_pipelineManager);

    // 连接调度器信号到结果处理器
    connect(m_pipelineManager->scheduler(), &PipelineScheduler::finished,
            m_pipelineResultHandler, &PipelineResultHandler::onPipelineResult);

    // 执行完成后切回配置模式
    connect(m_pipelineManager->scheduler(), &PipelineScheduler::finished,
            this, [this](const PipelineResult&) {
        m_pipelineMode = PipelineMode::Config;
    });

    // 执行状态变化时更新UI
    connect(m_pipelineManager->scheduler(), &PipelineScheduler::processingChanged,
            this, [this](bool processing) {
        if (processing) {
            ui->statusbar->showMessage("正在处理...");
        }
    });

    // 队列状态变化时更新UI
    connect(m_pipelineManager->scheduler(), &PipelineScheduler::queueChanged,
            this, [this](int pendingCount) {
        if (pendingCount > 0) {
            qDebug() << "[MainWindow] 队列中有" << pendingCount << "个待处理请求";
        }
    });

    connect(m_pipelineResultHandler, &PipelineResultHandler::statusMessage,
            ui->statusbar, &QStatusBar::showMessage);
}

// ========== 核心处理 ==========

void MainWindow::processAndDisplay()
{
    // 脏标记检查：没有变化时跳过Pipeline
    if (!m_needRefresh) return;
    m_needRefresh = false;

    m_displayModeManager->applyModeForCurrentTab();

    cv::Mat currentImage = m_roiManager.getCurrentImage();
    if (currentImage.empty()) return;

    qDebug() << "[MainWindow] processAndDisplay: img=" << currentImage.cols << "x" << currentImage.rows
             << "brightness=" << m_pipelineManager->config().enhance.brightness
             << "contrast=" << m_pipelineManager->config().enhance.contrast
             << "gamma=" << m_pipelineManager->config().enhance.gamma
             << "sharpen=" << m_pipelineManager->config().enhance.sharpen;

    ui->statusbar->showMessage("正在处理...");

    // 使用调度器异步执行
    PipelineConfig configSnapshot = m_pipelineManager->getConfigSnapshot();
    m_pipelineManager->scheduler()->submit(currentImage, configSnapshot);
}

void MainWindow::showImage(const cv::Mat &img)
{
    if (img.empty()) {
        // [NOTE] 画布清空：没有图片时清空所有显示状态和Tab结果
        m_view->clear();
        m_pipelineManager->clearLastResult();
        m_tabManager->clearAllResults();
    } else {
        m_view->setImage(ImageUtils::matToQImage(img));
    }
}

// ========== 文件操作 ==========

void MainWindow::on_btn_pipelineConfig_clicked()
{
    // [FIX] 加载当前图片的Pipeline配置（包括步骤组合）
    QString currentImageId = m_roiManager.getCurrentImageId();
    if (!currentImageId.isEmpty()) {
        PipelineConfig imageConfig = m_roiManager.loadImagePipelineConfig(currentImageId);
        m_pipelineManager->setConfig(imageConfig);
        m_pipelineManager->rebuildPipeline();
        // 通知所有Tab更新UI（包括StepConfigWidget）
        m_roiUiController->notifyConfigChanged(imageConfig);
    }

    // 跳转到「步骤」Tab（Pipeline配置页面）
    ensureTabExists("步骤");

    for (int i = 0; i < ui->tabWidget->count(); ++i) {
        if (ui->tabWidget->tabText(i) == "步骤") {
            ui->tabWidget->setTabVisible(i, true);  // 先显示Tab
            ui->tabWidget->setCurrentIndex(i);
            break;
        }
    }
}



void MainWindow::on_btn_cloudPlatform_clicked()
{
    m_cloudDashboardManager->launch();
}

// ========== ROI操作 ==========

void MainWindow::on_btn_addROI_clicked()      { m_roiUiController->onAddRoiClicked(); }
void MainWindow::on_btn_deleteROI_clicked()   { m_roiUiController->onDeleteRoiClicked(); }
void MainWindow::on_btn_switchROI_clicked()   { m_roiUiController->onSwitchRoiClicked(); }

// ========== Tab切换 ==========

void MainWindow::on_tabWidget_currentChanged(int index)
{
    if (m_isDestroying) return;
    if (m_roiManager.getFullImage().empty()) return;

    m_displayModeManager->onTabChanged(index);
}

// ========== 页面导航 ==========

void MainWindow::on_btn_Log_clicked()
{
    animatePageSwitch(0, 1);
}

void MainWindow::on_btn_Home_clicked()
{
    animatePageSwitch(1, 0);
}

// ========== 配置管理 ==========

void MainWindow::on_btn_saveConfig_clicked()  { m_configController->saveConfig(this); }
void MainWindow::on_btn_importConfig_clicked() { m_configController->loadConfig(this); }

// ========== 自动检测 ==========

void MainWindow::startAutoDetection()
{
    m_roiUiController->saveCurrentRoiPipelineConfig();
    m_autoDetectionController->startDetection();
}

void MainWindow::on_btn_startAutoDetection_clicked()
{
    startAutoDetection();
}

void MainWindow::on_btn_stopAutoDetection_clicked()
{
    m_autoDetectionController->stopDetection();
}

// ========== MQTT 边云协同 ==========

void MainWindow::setupMqtt()
{
    m_mqttManager = new MqttManager(this);
    m_mqttManager->initializeFromConfig();
    m_mqttManager->setupLogging();

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
        m_toast->showMessage("收到云端指令: 采集检测");
        
        cv::Mat frame;
        
        // 1. 优先从已打开的VideoManager获取新帧
        VideoTabWidget* videoTab = m_tabManager->getTabAs<VideoTabWidget>("视频");
        if (videoTab) {
            VideoManager* vm = videoTab->getVideoManager();
            if (vm && vm->isOpened()) {
                frame = vm->getNextFrame();
            }
        }
        
        // 2. 如果VideoManager未打开或获取失败，自动打开相机捕获一帧
        if (frame.empty()) {
            m_toast->showMessage("正在自动打开相机捕获...");
            
            try {
                cv::VideoCapture tempCapture;
                // 尝试DirectShow后端（Windows兼容性更好）
                tempCapture.open(1, cv::CAP_DSHOW);  // 使用USB摄像头(索引1)
                
                if (!tempCapture.isOpened()) {
                    tempCapture.open(1);  // 回退到默认后端
                }
                
                if (tempCapture.isOpened()) {
                    // 等待相机稳定（约300ms）
                    QThread::msleep(300);
                    
                    cv::Mat rawFrame;
                    if (tempCapture.read(rawFrame)) {
                        // 相机源左右翻转，提升体验
                        cv::flip(rawFrame, frame, 1);
                    }
                    tempCapture.release();
                }
            } catch (const cv::Exception& ex) {
                Logger::instance()->error(QString("自动打开相机失败: %1").arg(ex.what()));
            }
        }
        
        // 3. 检查是否成功获取帧
        if (frame.empty()) {
            m_toast->showMessage("错误：无法获取图像，请手动打开相机");
            return;
        }
        
        // 4. 将帧存入RoiManager并触发检测
        m_roiManager.setFullImage(frame);
        
        // 为capture采集的图像自动添加全图ROI配置（如果还没有ROI配置）
        QString currentImageId = m_roiManager.getCurrentImageId();
        if (!currentImageId.isEmpty() && m_roiManager.getRoiConfigsForImage(currentImageId).isEmpty()) {
            RoiConfig defaultRoi;
            defaultRoi.roiId = "capture_default_roi";
            defaultRoi.roiName = "全图检测区域";
            defaultRoi.roiRect = QRectF(0, 0, 1, 1);  // 全图（归一化坐标）
            defaultRoi.pipelineConfig = m_pipelineManager->getConfigSnapshot();
            defaultRoi.isActive = true;
            m_roiManager.addRoiConfig(defaultRoi);
            Logger::instance()->info("[capture] 已为采集的图像添加默认全图ROI配置");
        }
        
        m_toast->showMessage("已采集新帧，正在处理...");
        requestRefresh();
    });
    connect(m_mqttManager, &MqttManager::startDetectionRequested,
            this, [this]() {
        m_toast->showMessage("收到云端指令: 开始批量检测");
        startAutoDetection();
    });
    connect(m_mqttManager, &MqttManager::stopDetectionRequested,
            this, [this]() {
        m_toast->showMessage("收到云端指令: 停止批量检测");
        on_btn_stopAutoDetection_clicked();
    });
    connect(m_mqttManager, &MqttManager::pingRequested,
            this, [this]() {
        m_toast->showMessage("收到云端指令: Ping - 已回复心跳");
    });
}

// ========== Tab懒加载 ==========

void MainWindow::ensureTabExists(const QString& tabName)
{
    if (m_tabManager->isCreated(tabName)) return;

    m_tabManager->ensureTab(tabName);

    m_detectionUiController->setTabNames(m_tabManager->createdTabNames());
}

// ========== 刷新请求 ==========

void MainWindow::requestRefresh()
{
    m_needRefresh = true;
    // 调度器内部已有消抖机制，直接触发
    processAndDisplay();
}

// ========== 方案操作 ==========

void MainWindow::on_btn_saveToProfile_clicked()
{
    m_profileController->saveToProfile();
}

void MainWindow::on_btn_loadFromProfile_clicked()
{
    m_profileController->loadFromProfile();
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

void MainWindow::hideAllDetectionTabs()
{
    if (!ui->tabWidget) return;

    // 隐藏所有Tab（包括"图像"Tab）
    for (int i = 0; i < ui->tabWidget->count(); ++i) {
        ui->tabWidget->setTabVisible(i, false);
    }

    Logger::instance()->info("[MainWindow] 已隐藏所有Tab");
}


