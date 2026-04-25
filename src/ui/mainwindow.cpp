#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "logger.h"
#include "config/constants.h"
#include <QFile>
#include <QDir>
#include <QTimer>
#include <QInputDialog>
#include <QDebug>
#include <QShortcut>
#include <QComboBox>
#include <QStandardItemModel>
#include <QMenu>
#include <QVBoxLayout>
#include <QLabel>
#include <QDialogButtonBox>
#include <QContextMenuEvent>
#include "controllers/roi_ui_controller.h"
#include "controllers/detection_ui_controller.h"
#include "controllers/config_controller.h"
#include "signal_connector.h"

// Tab Widget头文件（mainwindow.cpp中仍需使用具体类型的方法）
#include "widgets/judge_tab_widget.h"
#include "widgets/line_tab_widget.h"
#include "widgets/barcode_tab_widget.h"
#include "widgets/object_detection_tab_widget.h"
#include "widgets/image_tab_widget.h"
#include "widgets/video_tab_widget.h"
#include "widgets/enhance_tab_widget.h"
#include "widgets/filter_tab_widget.h"
#include "widgets/template_tab_widget.h"
#include "widgets/process_tab_widget.h"
#include "widgets/extract_tab_widget.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , m_pipelineManager(new PipelineManager(this))
    , m_systemMonitor(new SystemMonitor(this))
    , m_fileManager(new FileManager(this))
    , m_processDebounceTimer(nullptr)
    , m_currentTabIndex(0)
{
    ui->setupUi(this);
    
    // 设置窗口最大化启动
    setWindowState(Qt::WindowMaximized);
    showMaximized();

    m_processDebounceTimer =new QTimer(this);
    m_processDebounceTimer ->setSingleShot(true);
    m_processDebounceTimer ->setInterval(AppConstants::DEBOUNCE_PROCESS_MS);

    connect(m_processDebounceTimer, &QTimer::timeout,this,&MainWindow::processAndDisplay);

    setupUI();
    setupConnections();

    setupSystemMonitor();

    // 设置日志文件路径（使用项目根目录）
    QString logDir = PROJECT_ROOT_DIR "/logs";
    QDir dir(logDir);
    if (!dir.exists()) {
        dir.mkpath(".");
    }
    QString logPath = logDir + "/test.log";
    Logger::instance()->setLogFile(logPath);
    Logger::instance()->enableFileLog(true);
    
    // 初始化日志页面
    ui->page_log->initialize();
    
    // 连接日志页面的返回主页信号
    connect(ui->page_log, &LogPage::requestGoHome, this, [this]() {
        ui->stackedWidget_main->setCurrentIndex(0);
    });
    
    // ========== Tab懒加载：启动时不创建任何Tab Widget ==========
    // 初始化TabManager
    m_tabManager = new TabManager(ui->tabWidget, m_pipelineManager, &m_roiManager,
                                  m_view, m_processDebounceTimer, this);

    // 初始化Controller对象
    m_roiUiController = new RoiUiController(m_roiManager, m_pipelineManager, m_view, ui->statusbar, this);
    m_detectionUiController = new DetectionUiController(m_roiManager, ui->tabWidget, this);
    m_configController = new ConfigController(m_pipelineManager, m_roiManager, this);

    // 初始化自动检测控制器
    m_autoDetectionController = new AutoDetectionController(m_pipelineManager, &m_roiManager, this);
    m_autoDetectionController->setupUiConnections(
        ui->btn_startAutoDetection, ui->btn_stopAutoDetection,
        ui->statusbar, ui->label_imageList);
    
    // 设置Controller的Tab名称列表（初始为空，后续动态添加）
    m_detectionUiController->setTabNames(m_tabNames);
    
    // 设置ConfigController的Tab Widget（初始为nullptr，Tab创建后更新）
    m_configController->setTabWidgets(nullptr, nullptr, nullptr, nullptr, nullptr);
    
    // 初始化ROI UI Controller的TreeView
    m_roiUiController->setupTreeView(ui->treeView);
    
    // 连接Controller信号（从各Controller的setupConnections中建立）
    // 【P1修复】roiChanged 仅用于需要重新Pipeline处理的场景（添加/删除/修改ROI配置）
    connect(m_roiUiController, &RoiUiController::roiChanged, this, &MainWindow::processAndDisplay);
    connect(m_configController, &ConfigController::configApplied, this, &MainWindow::processAndDisplay);

    // 各Controller内部信号连接（从MainWindow迁移）
    m_roiUiController->setupMainWindowConnections(m_tabManager);
    m_detectionUiController->setupConnections(m_roiUiController,
        [this](const QString& tabName) { ensureTabExists(tabName); });

    // 初始化信号连接器
    m_signalConnector = new SignalConnector(this, m_pipelineManager, &m_roiManager,
                                           m_view, m_tabManager, m_roiUiController,
                                           m_detectionUiController, this);

    // 连接TabManager的tabCreated信号，委托给SignalConnector建立信号连接
    connect(m_tabManager, &TabManager::tabCreated,
            m_signalConnector, &SignalConnector::connectTabSignals);

    // 初始化图片列表管理器
    m_imageListManager = new ImageListManager(m_roiManager, m_fileManager, this, this);
    m_imageListManager->init(ui->listWidget_images, ui->btn_addImage, ui->btn_removeImage);
    connect(m_imageListManager, &ImageListManager::imageDisplayRequested, this, &MainWindow::showImage);
    connect(m_imageListManager, &ImageListManager::processRequested, this, &MainWindow::processAndDisplay);

    // 连接图片切换信号：当切换图片时，自动同步ROI配置到UI
    connect(&m_roiManager, &RoiManager::currentImageChanged, this, [this](const QString& imageId) {
        Q_UNUSED(imageId);
        m_roiUiController->syncRoiConfigsToWidget();
    });

    // 连接检测项选中信号到Tab切换逻辑
    connect(m_roiUiController, &RoiUiController::detectionItemSelected, 
            this, [this](const QString& roiId, const QString& detectionId) {
        // 获取ROI和检测项
        RoiConfig* roi = m_roiManager.getRoiConfig(roiId);
        if (roi) {
            for (const auto& detection : roi->detectionItems) {
                if (detection.itemId == detectionId) {
                    TabConfig config = TabConfig::getConfigForType(detection.type);
                    m_detectionUiController->switchToTabConfig(config);

                    // 【Bug1修复补充】当选中Blob检测项时，将DetectionItem.config中的BlobAnalysisConfig
                    // 加载到JudgeTabWidget，确保判定Tab的min/max值与DetectionItem.config同步
                    if (detection.type == DetectionType::Blob) {
                        BlobAnalysisConfig blobConfig;
                        blobConfig.fromJson(detection.config);
                        if (auto* judgeTab = m_tabManager->getJudgeTab()) {
                            judgeTab->setJudgeConfig(
                                blobConfig.minBlobCount,
                                blobConfig.maxBlobCount,
                                m_pipelineManager->getLastContext().currentRegions
                            );
                            Logger::instance()->info(QString("[MainWindow] 从DetectionItem加载Blob判定配置到JudgeTab: min=%1, max=%2")
                                .arg(blobConfig.minBlobCount).arg(blobConfig.maxBlobCount));
                        }
                    }
                    break;
                }
            }
        }
    });
    
    // 连接添加/删除检测项按钮
    connect(ui->btn_addDetection, &QPushButton::clicked, this, [this]() {
        m_detectionUiController->onAddDetectionClicked(m_roiUiController->getCurrentSelectedRoiId());
    });
    connect(ui->btn_deleteDetection, &QPushButton::clicked, this, &MainWindow::onDeleteDetectionClicked);

    // ✅ 修复：PipelineResultHandler 放在最后初始化
    // 此时 m_view, m_tabManager, m_pipelineManager 全部已经初始化完成
    m_pipelineResultHandler = new PipelineResultHandler(this);
    m_pipelineResultHandler->setDependencies(m_tabManager, &m_roiManager, m_view, m_pipelineManager);
    m_pipelineResultHandler->watchPipeline(&m_pipelineWatcher);
    
    connect(m_pipelineResultHandler, &PipelineResultHandler::statusMessage,
            ui->statusbar, &QStatusBar::showMessage);
            
    connect(m_pipelineResultHandler, &PipelineResultHandler::processingFinished,
            this, [this]() {
                m_isProcessing = false;
                // 检查是否有pending的处理请求
                if (m_hasPendingProcess) {
                    qDebug() << "检测到pending处理请求，重新触发processAndDisplay";
                    m_hasPendingProcess = false;
                    QTimer::singleShot(AppConstants::PENDING_PROCESS_DELAY_MS, this, &MainWindow::processAndDisplay);
                }
            });
}

MainWindow::~MainWindow()
{
    m_isDestroying = true;

    // 【关键修复】等待后台Pipeline处理完成，避免析构时访问已释放的cv::Mat/OpenCV资源
    if (m_isProcessing) {
        m_pipelineWatcher.waitForFinished();
    }

    // 断开所有可能在析构过程中触发的信号连接
    disconnect(&m_pipelineWatcher, nullptr, this, nullptr);
    disconnect(Logger::instance(), nullptr, this, nullptr);

    delete ui;
}

void MainWindow::setupSystemMonitor()
{
    // 系统监控信息显示在状态栏
    m_systemMonitor->setupWithStatusBar(ui->statusbar, AppConstants::SYSTEM_MONITOR_INTERVAL_MS);
}

// ========== 初始化 ==========

void MainWindow::setupUI()
{
    m_view = static_cast<ImageView*>(ui->graphicsView);

    // 加载样式表
    QFile file(":/style.qss");
    if (file.open(QFile::ReadOnly))
    {
        QString style = QLatin1String(file.readAll());
        qApp->setStyleSheet(style);
    }
    
    // 初始化图片列表
    // （已由 ImageListManager 在构造函数中完成）
}

void MainWindow::setupConnections()
{
    // 连接tabWidget的currentChanged信号
    connect(ui->tabWidget, &QTabWidget::currentChanged, this, &MainWindow::on_tabWidget_currentChanged);
    
    // ========== FileManager信号 ==========
    connect(m_fileManager, &FileManager::imageLoaded,
            this, [this](const cv::Mat& img) {
                // 设置完整图像
                m_roiManager.setFullImage(img);
                m_view->clearRoi();
                m_pipelineManager->resetPipeline();

                // 显示图像
                showImage(img);
                Logger::instance()->info("图像加载成功!");
            });

    connect(m_fileManager, &FileManager::imageSaved,
            this, [this](const QString& path) {
                Logger::instance()->info(QString("图像保存成功: %1").arg(path));
            });

    connect(m_fileManager, &FileManager::errorOccurred,
            this, [this](const QString& message) {
                Logger::instance()->error(message);
                QMessageBox::warning(this, "错误", message);
            });

    // ========== ImageView信号 ==========
    connect(m_view, &ImageView::pixelInfoChanged, this,
            [this](int x, int y, const QColor &color, int gray) {
                ui->statusbar->showMessage(
                    QString("X=%1 Y=%2  R=%3 G=%4 B=%5  Gray=%6")
                        .arg(x).arg(y)
                        .arg(color.red()).arg(color.green()).arg(color.blue())
                        .arg(gray)
                    );
                QTimer::singleShot(AppConstants::PIXEL_INFO_TIMEOUT_MS, ui->statusbar, &QStatusBar::clearMessage);
            });

    // roiSelected信号已在RoiUiController中处理，此处不再重复连接（避免双重创建ROI）
    // MainWindow::onRoiSelected 已由 RoiUiController 内部调用

    connect(m_view, &ImageView::polygonDrawingPointAdded, this,
            [](const QString& type, const QPointF& point) {
                Logger::instance()->info(
                    QString("[%1] 添加顶点: (%2, %3)")
                        .arg(type)
                        .arg(point.x(), 0, 'f', 1)
                        .arg(point.y(), 0, 'f', 1)
                    );
            });
}

// ========== 核心处理 ==========

void MainWindow::processAndDisplay()
{
    // 如果正在处理，设置pending标志，稍后自动重新触发
    if (m_isProcessing) {
        m_hasPendingProcess = true;
        return;
    }

    // ========== 设置显示模式 ==========
    setDisplayModeForCurrentTab();

    // ========== 获取当前图像 ==========
    cv::Mat currentImage = m_roiManager.getCurrentImage();
    if (currentImage.empty()) {
        return;
    }

    // ========== 使用QtConcurrent在后台线程执行Pipeline ==========
    m_isProcessing = true;
    m_hasPendingProcess = false;
    ui->statusbar->showMessage("正在处理...");

    // 在主线程获取配置快照（值拷贝），确保后台线程使用一致的配置
    PipelineConfig configSnapshot = m_pipelineManager->getConfigSnapshot();

    QFuture<PipelineContext> future = QtConcurrent::run(
        [this, currentImage, configSnapshot]() -> PipelineContext {
            return m_pipelineManager->execute(currentImage, configSnapshot);
        }
    );
    m_pipelineWatcher.setFuture(future);
}

// ========== Tab名称 → 显示模式 静态映射 ==========
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
    // 获取当前Tab名称
    QString tabName;
    if (m_currentTabIndex >= 0 && m_currentTabIndex < m_tabNames.size()) {
        tabName = m_tabNames[m_currentTabIndex];
    }

    // 查表获取显示模式
    DisplayConfig::Mode mode = s_tabDisplayMode.value(tabName, DisplayConfig::Mode::Original);

    // 过滤模式特殊处理（需要根据当前过滤状态动态决定）
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
    QImage qimg = ImageUtils::matToQImage(img);
    m_view->setImage(qimg);
}

// ========== 文件操作 ==========
void MainWindow::on_btn_openImg_clicked()
{
    // 优先使用项目根目录的images文件夹，如果不存在则使用应用目录
    QString path = QCoreApplication::applicationDirPath() + "/../../images/";
    QDir dir(path);
    if (!dir.exists()) {
        path = QCoreApplication::applicationDirPath() + "/images/";
    }

    QString fileName = m_fileManager->selectImageFile(path);
    if (fileName.isEmpty()) {
        Logger::instance()->warning("用户取消了文件选择");
        return;
    }

    m_fileManager->readImageFile(fileName);
}

void MainWindow::on_btn_openVideo_clicked()
{
    Logger::instance()->info(QString("========== 主窗口视频打开流程 =========="));
    
    // 切换到视频Tab
    if (auto* videoTab = m_tabManager->getVideoTab()) {
        Logger::instance()->info("视频Tab组件存在");
        
        // 使用视频管理器打开视频文件
        VideoManager* videoManager = videoTab->getVideoManager();
        if (videoManager) {
            Logger::instance()->info("视频管理器获取成功");
            
            // 打开视频文件选择对话框
            QString filePath = QFileDialog::getOpenFileName(
                this,
                "选择视频文件",
                "",
                "视频文件 (*.mp4 *.avi *.mov *.mkv *.wmv);;所有文件 (*.*)"
            );

            if (!filePath.isEmpty()) {
                Logger::instance()->info(QString("用户选择视频文件: %1").arg(filePath));
                
                // 获取文件信息
                QFileInfo fileInfo(filePath);
                Logger::instance()->info(QString("文件大小: %1 字节").arg(fileInfo.size()));
                
                // 使用视频管理器打开文件
                if (videoManager->openFile(filePath)) {
                    Logger::instance()->info("视频管理器打开文件成功");
                    
                    // 切换到视频Tab
                    int videoTabIndex = ui->tabWidget->indexOf(videoTab);
                    if (videoTabIndex >= 0) {
                        ui->tabWidget->setCurrentIndex(videoTabIndex);
                        Logger::instance()->info(QString("已切换到视频Tab (索引: %1)").arg(videoTabIndex));
                    } else {
                        Logger::instance()->warning("未找到视频Tab的索引");
                    }
                    
                    Logger::instance()->info(QString("打开视频成功: %1").arg(filePath));
                } else {
                    Logger::instance()->error(QString("视频管理器打开文件失败: %1").arg(filePath));
                }
            } else {
                Logger::instance()->info("用户取消了文件选择");
            }
        } else {
            Logger::instance()->error("无法获取视频管理器");
        }
    } else {
        Logger::instance()->error("视频Tab组件不存在");
    }
    
    Logger::instance()->info(QString("========== 主窗口视频打开流程结束 =========="));
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

void MainWindow::on_btn_resetROI_clicked()
{
    m_roiUiController->onResetRoiClicked();
    // 移除 processAndDisplay() — roiChanged 信号已连接到 processAndDisplay
}

void MainWindow::onRoiSelected(const QRectF &roiImgRectF)
{
    m_roiUiController->handleRoiSelectedComplete(roiImgRectF);
    // 移除 processAndDisplay() — roiChanged 信号已连接到 processAndDisplay
}

void MainWindow::on_btn_addROI_clicked()
{
    m_roiUiController->onAddRoiClicked();
}

void MainWindow::on_btn_deleteROI_clicked()
{
    m_roiUiController->onDeleteRoiClicked();
    // 移除 processAndDisplay() — roiChanged 信号已连接到 processAndDisplay
}

void MainWindow::on_btn_switchROI_clicked()
{
    m_roiUiController->onSwitchRoiClicked();
    // 【P1修复】移除 processAndDisplay() — 切换显示是纯UI操作
    // onSwitchRoiClicked 内部已经通过 roiDisplayChanged/fullImageDisplayRequested 信号处理显示
    // 不需要重新执行Pipeline处理
}

void MainWindow::on_tabWidget_currentChanged(int index)
{
    // 防止程序关闭时访问已销毁的UI对象
    if (m_isDestroying) {
        return;
    }

    if(m_roiManager.getFullImage().empty()) return;

    m_currentTabIndex = index;
    processAndDisplay();
}

void MainWindow::on_btn_Log_clicked()
{
    // 切换到日志页面（全屏显示）
    if (ui->stackedWidget_main) {
        ui->stackedWidget_main->setCurrentIndex(1); // 切换到日志页面
    }
    
    // 不再清空日志，让Logger自动追加新日志到QTextEdit
}

void MainWindow::on_btn_Home_clicked()
{
    // 切换回主页
    if (ui->stackedWidget_main) {
        ui->stackedWidget_main->setCurrentIndex(0); // 切换到主页
    }
}

void MainWindow::on_btn_saveConfig_clicked()
{
    saveConfig();
}

void MainWindow::on_btn_importConfig_clicked()
{
    loadConfig();
}

// ========== 自动检测 ==========

void MainWindow::on_btn_startAutoDetection_clicked()
{
    // 【Bug修复】确保当前ROI的最新Pipeline配置（包含shapeFilter等）已保存到RoiConfig中
    // 用户可能在Extract/Filter/Enhance等tab中修改了参数但未切换ROI，导致最新参数未同步到RoiConfig
    m_roiUiController->saveCurrentRoiPipelineConfig();
    
    // 从 RoiManager 自动获取已添加的图片和ROI配置，无需弹文件选择对话框
    m_autoDetectionController->startDetection();
}

void MainWindow::on_btn_stopAutoDetection_clicked()
{
    m_autoDetectionController->stopDetection();
}

// ========== 配置管理 ==========

void MainWindow::saveConfig()
{
    m_configController->saveConfig(this);
}

void MainWindow::loadConfig()
{
    m_configController->loadConfig(this);
}

void MainWindow::collectConfigFromUI(AppConfig& config)
{
    // 这个方法现在由ConfigController处理
    // 保留为空方法以避免编译错误
}

void MainWindow::applyConfigToUI(const AppConfig& config)
{
    // 这个方法现在由ConfigController处理
    // 保留为空方法以避免编译错误
}

// ========== 检测项管理已委托给Controller ==========

void MainWindow::onDeleteDetectionClicked()
{
    // 检查树形视图当前选中的项
    QTreeWidgetItem* currentItem = ui->treeView->currentItem();
    if (!currentItem) {
        QMessageBox::warning(this, "警告", "请先选择要删除的检测项");
        return;
    }
    
    // 获取选中项的类型
    QString itemType = currentItem->data(0, Qt::UserRole + 1).toString();
    
    // 如果选中的是检测项
    if (itemType == "detection") {
        QString detectionId = currentItem->data(0, Qt::UserRole).toString();
        QTreeWidgetItem* parentItem = currentItem->parent();
        
        if (parentItem) {
            QString roiId = parentItem->data(0, Qt::UserRole).toString();
            m_roiUiController->deleteDetectionItem(roiId, detectionId);
        }
    } else {
        // 选中的是ROI，提示用户选择检测项
        QMessageBox::warning(this, "警告", "请先选择要删除的检测项");
    }
}

// ========== Tab懒加载（委托给TabManager）==========

void MainWindow::ensureTabExists(const QString& tabName)
{
    if (m_tabManager->isCreated(tabName)) return;

    m_tabManager->ensureTab(tabName);

    // 更新m_tabNames（保持与TabManager同步）
    m_tabNames = m_tabManager->createdTabNames();

    // 更新DetectionUiController的Tab名称列表
    m_detectionUiController->setTabNames(m_tabNames);

    // 更新ConfigController的Tab Widget指针
    m_configController->setTabWidgets(
        m_tabManager->getEnhanceTab(),
        m_tabManager->getFilterTab(),
        m_tabManager->getExtractTab(),
        m_tabManager->getJudgeTab(),
        m_tabManager->getProcessTab()
    );
}

void MainWindow::connectTabSignals(const QString& tabName, QWidget* widget)
{
    // 委托给 SignalConnector 处理
    // 保留此方法以兼容 TabManager::tabCreated 信号签名
    if (m_signalConnector) {
        m_signalConnector->connectTabSignals(tabName, widget);
    }
}

