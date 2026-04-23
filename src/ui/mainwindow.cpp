#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "logger.h"
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
    m_processDebounceTimer ->setInterval(150);

    connect(m_processDebounceTimer, &QTimer::timeout,this,&MainWindow::processAndDisplay);

    // 连接多线程处理完成信号
    connect(&m_pipelineWatcher, &QFutureWatcher<PipelineContext>::finished, 
            this, [this]() {
                m_isProcessing = false;
                PipelineContext result = m_pipelineWatcher.result();
                // 获取Pipeline输出的干净图像（副本）
                cv::Mat displayImage = result.getFinalDisplay();

                // 更新判定Tab显示
                int count = result.currentRegions;
                if (auto* judgeTab = m_tabManager->getJudgeTab()) {
                    judgeTab->setCurrentRegionCount(count);
                }
                
                // 更新直线检测Tab匹配结果
                if (auto* lineTab = m_tabManager->getLineDetectTab(); lineTab && result.totalLineCount > 0) {
                    const PipelineConfig& cfg = m_pipelineManager->getConfig();
                    lineTab->updateMatchResultStatus(
                        result.matchedLineCount, 
                        result.totalLineCount, 
                        cfg.angleThreshold, 
                        cfg.distanceThreshold
                    );
                }
                
                // 更新条码Tab识别结果
                if (auto* barcodeTab = m_tabManager->getBarcodeTab()) {
                    barcodeTab->updateResultsTable(result.barcodeResults);
                    barcodeTab->updateStatus(result.barcodeStatus);
                }

                // 执行目标检测（模型已加载时）
                if (auto* objTab = m_tabManager->getObjectDetectionTab(); objTab && objTab->isModelLoaded()) {
                    cv::Mat detectImage = m_roiManager.getCurrentImage();
                    if (!detectImage.empty()) {
                        std::vector<DetectionResult> detResults = objTab->runDetection(detectImage);
                        objTab->updateDetectionResults(detResults);

                        // 在干净的displayImage上绘制检测框
                        for (const auto& det : detResults) {
                            cv::rectangle(displayImage, det.box, cv::Scalar(0, 255, 0), 2);

                            std::string label = det.className + " " + cv::format("%.2f", det.confidence);
                            int baseline = 0;
                            cv::Size textSize = cv::getTextSize(label, cv::FONT_HERSHEY_SIMPLEX, 0.6, 1, &baseline);

                            cv::Point textOrg(det.box.x, det.box.y - 5);
                            if (textOrg.y - textSize.height < 0) {
                                textOrg.y = det.box.y + textSize.height + 5;
                            }
                            cv::rectangle(displayImage,
                                cv::Point(textOrg.x, textOrg.y - textSize.height - 2),
                                cv::Point(textOrg.x + textSize.width + 2, textOrg.y + 4),
                                cv::Scalar(0, 255, 0), -1);

                            cv::putText(displayImage, label, textOrg,
                                cv::FONT_HERSHEY_SIMPLEX, 0.6, cv::Scalar(0, 0, 0), 1);
                        }
                    }
                }

                // 绘制完成后再显示图像
                showImage(displayImage);
                
                // 恢复状态栏
                ui->statusbar->showMessage("处理完成", 2000);
                
                // 检查是否有pending的处理请求（避免滑条快速拖动时丢失处理）
                if (m_hasPendingProcess) {
                    qDebug() << "检测到pending处理请求，重新触发processAndDisplay";
                    m_hasPendingProcess = false;
                    // 使用单次定时器确保UI更新完成后再处理
                    QTimer::singleShot(50, this, &MainWindow::processAndDisplay);
                }
            });

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
    
    // 设置Controller的Tab名称列表（初始为空，后续动态添加）
    m_detectionUiController->setTabNames(m_tabNames);
    
    // 设置ConfigController的Tab Widget（初始为nullptr，Tab创建后更新）
    m_configController->setTabWidgets(nullptr, nullptr, nullptr, nullptr, nullptr);
    
    // 初始化ROI UI Controller的TreeView
    m_roiUiController->setupTreeView(ui->treeView);
    
    // 连接Controller信号
    connect(m_roiUiController, &RoiUiController::roiChanged, this, &MainWindow::processAndDisplay);
    connect(m_detectionUiController, &DetectionUiController::detectionChanged, this, [this]() {
        m_roiUiController->refreshRoiTreeView();
    });
    connect(m_configController, &ConfigController::configApplied, this, &MainWindow::processAndDisplay);
    
    // 连接ROI切换信号：当切换ROI时，更新EnhanceTabWidget的UI显示
    connect(m_roiUiController, &RoiUiController::roiPipelineConfigChanged, 
            this, [this](const PipelineConfig& config) {
        if (auto* enhanceTab = m_tabManager->getEnhanceTab()) {
            enhanceTab->setEnhanceConfig(
                config.brightness,
                static_cast<int>(config.contrast * 100),
                static_cast<int>(config.gamma * 100),
                static_cast<int>(config.sharpen * 100)
            );
        }
    });
    
    // 连接Tab懒加载信号
    connect(m_detectionUiController, &DetectionUiController::ensureTabNeeded,
            this, &MainWindow::ensureTabExists);

    // 连接TabManager的tabCreated信号，在此处建立Tab与MainWindow的信号连接
    connect(m_tabManager, &TabManager::tabCreated,
            this, &MainWindow::connectTabSignals);

    // 初始化图片列表管理器
    m_imageListManager = new ImageListManager(m_roiManager, m_fileManager, this, this);
    m_imageListManager->init(ui->listWidget_images, ui->btn_addImage, ui->btn_removeImage);
    connect(m_imageListManager, &ImageListManager::imageDisplayRequested, this, &MainWindow::showImage);
    connect(m_imageListManager, &ImageListManager::processRequested, this, &MainWindow::processAndDisplay);

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

}

MainWindow::~MainWindow()
{
    m_isDestroying = true;
    // 先断开Logger的所有连接，防止析构时访问已删除的UI
    disconnect(Logger::instance(), nullptr, this, nullptr);
    delete ui;
}

void MainWindow::setupSystemMonitor()
{
    // 系统监控信息显示在状态栏
    m_systemMonitor->setupWithStatusBar(ui->statusbar, 1000);
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
                QTimer::singleShot(5000, ui->statusbar, &QStatusBar::clearMessage);
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

    QFuture<PipelineContext> future = QtConcurrent::run(
        [this, currentImage]() -> PipelineContext {
            return m_pipelineManager->execute(currentImage);
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
        const PipelineConfig& cfg = m_pipelineManager->getConfig();
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
    m_roiManager.resetRoiWithUI(m_view, ui->statusbar);
    processAndDisplay();  // 立即更新显示
}

void MainWindow::onRoiSelected(const QRectF &roiImgRectF)
{
    if (!m_roiManager.handleRoiSelected(roiImgRectF, ui->statusbar)) {
        return;
    }

    // 绘制完ROI后重置图像缩放，恢复到原始比例
    m_view->resetZoom();

    processAndDisplay();
}

void MainWindow::on_btn_addROI_clicked()
{
    // 检查是否已加载图像
    if (m_roiManager.getFullImage().empty()) {
        QMessageBox::warning(this, "警告", "请先加载一张图像");
        return;
    }
    
    // 提示用户绘制ROI
    Logger::instance()->info("请在图像上绘制新的ROI");
    m_roiManager.enableDrawRoiMode(m_view, ui->statusbar);
}

void MainWindow::on_btn_deleteROI_clicked()
{
    // 委托给RoiUiController处理
    // 这里需要通过Controller的公共方法来处理删除
    // 暂时保留原有的删除逻辑，但使用Controller的刷新方法
    
    // 检查是否有选中的ROI
    QTreeWidgetItem* currentItem = ui->treeView->currentItem();
    if (!currentItem) {
        QMessageBox::warning(this, "警告", "请先选择要删除的ROI");
        return;
    }
    
    // 获取ROI ID
    QString roiId = currentItem->data(0, Qt::UserRole).toString();
    QString itemType = currentItem->data(0, Qt::UserRole + 1).toString();
    
    // 如果选中的是检测项，需要找到其父ROI
    if (itemType == "detection") {
        QTreeWidgetItem* parentItem = currentItem->parent();
        if (parentItem) {
            roiId = parentItem->data(0, Qt::UserRole).toString();
        } else {
            QMessageBox::warning(this, "警告", "请先选择要删除的ROI");
            return;
        }
    }
    
    // 获取ROI配置
    RoiConfig* roi = m_roiManager.getRoiConfig(roiId);
    if (!roi) {
        QMessageBox::warning(this, "警告", "未找到选中的ROI");
        return;
    }
    
    // 确认删除
    QMessageBox::StandardButton reply = QMessageBox::question(
        this, "确认删除",
        QString("确定要删除ROI \"%1\" 及其所有检测项吗？").arg(roi->roiName),
        QMessageBox::Yes | QMessageBox::No
    );
    
    if (reply == QMessageBox::Yes) {
        // 删除ROI
        m_roiManager.removeRoiConfig(roiId);
        
        // 使用Controller刷新树形视图
        m_roiUiController->refreshRoiTreeView();
        
        // 重置ROI管理器
        m_roiManager.resetRoiWithUI(m_view, ui->statusbar);
        
        // 重新处理图像
        processAndDisplay();
        
        Logger::instance()->info(QString("已删除ROI: %1").arg(roi->roiName));
    }
}

void MainWindow::on_btn_switchROI_clicked()
{
    if (m_roiManager.getFullImage().empty()) {
        QMessageBox::warning(this, "警告", "请先加载一张图像");
        return;
    }
    
    if (m_roiManager.isRoiActive()) {
        // 当前是ROI模式，切换到原图模式
        // 先保存当前ROI的PipelineConfig
        m_roiUiController->saveCurrentRoiPipelineConfig();
        
        m_roiManager.resetRoi();
        m_view->clearRoi();
        m_view->resetZoom();
        m_view->setImage(ImageUtils::matToQImage(m_roiManager.getFullImage()));
        Logger::instance()->info("已切换到原图模式");
    } else {
        // 当前是原图模式，切换到ROI模式
        // 使用Controller获取当前选中的ROI ID
        QString currentRoiId = m_roiUiController->getCurrentSelectedRoiId();
        
        if (currentRoiId.isEmpty()) {
            QMessageBox::warning(this, "警告", "请先在左侧列表中选择一个ROI");
            return;
        }
        
        RoiConfig* roi = m_roiManager.getRoiConfig(currentRoiId);
        if (!roi) {
            QMessageBox::warning(this, "警告", "未找到选中的ROI");
            return;
        }
        
        // 加载该ROI的PipelineConfig到PipelineManager
        m_roiUiController->loadRoiPipelineConfig(currentRoiId);
        
        // 切换到ROI模式
        m_view->clearRoi();
        m_roiManager.setRoi(roi->roiRect);
        processAndDisplay();
        Logger::instance()->info(QString("已切换到ROI模式: %1").arg(roi->roiName));
    }
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

// ========== 检测项管理已移至Controller ==========

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
            RoiConfig* roi = m_roiManager.getRoiConfig(roiId);
            
            if (roi) {
                // 查找检测项名称用于确认提示
                QString detectionName;
                for (const auto& detection : roi->detectionItems) {
                    if (detection.itemId == detectionId) {
                        detectionName = detection.itemName;
                        break;
                    }
                }
                
                // 确认删除
                QMessageBox::StandardButton reply = QMessageBox::question(
                    this, "确认删除",
                    QString("确定要删除检测项 \"%1\" 吗？").arg(detectionName.isEmpty() ? detectionId : detectionName),
                    QMessageBox::Yes | QMessageBox::No
                );
                
                if (reply == QMessageBox::Yes) {
                    roi->removeDetectionItem(detectionId);
                    m_roiUiController->refreshRoiTreeView();
                    Logger::instance()->info(QString("已删除检测项: %1").arg(detectionName.isEmpty() ? detectionId : detectionName));
                }
            }
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
    // 根据Tab名称建立信号连接
    if (tabName == "图像") {
        auto* tab = qobject_cast<ImageTabWidget*>(widget);
        connect(tab, &ImageTabWidget::channelChanged,
                this, [this](int channel) {
                    m_pipelineManager->getConfig().channel =
                        static_cast<PipelineConfig::Channel>(channel);
                    processAndDisplay();
                });
    }
    else if (tabName == "视频") {
        auto* tab = qobject_cast<VideoTabWidget*>(widget);
        connect(tab, &VideoTabWidget::videoFrameReady,
                this, [this](const cv::Mat& frame) {
                    if (!frame.empty()) {
                        m_roiManager.setFullImage(frame);
                        m_view->clearRoi();
                        processAndDisplay();
                    }
                });
    }
    else if (tabName == "增强") {
        auto* tab = qobject_cast<EnhanceTabWidget*>(widget);
        connect(tab, &EnhanceTabWidget::processRequested,
                this, &MainWindow::processAndDisplay);
    }
    else if (tabName == "过滤") {
        auto* tab = qobject_cast<FilterTabWidget*>(widget);
        connect(tab, &FilterTabWidget::filterConfigChanged,
                this, &MainWindow::processAndDisplay);
    }
    else if (tabName == "补正") {
        auto* tab = qobject_cast<TemplateTabWidget*>(widget);
        connect(tab, &TemplateTabWidget::imageToShow,
                this, &MainWindow::showImage);
        connect(tab, &TemplateTabWidget::templateCreated,
                this, [](const QString& n) {
                    Logger::instance()->info(QString("模板已创建: %1").arg(n));
                });
        connect(tab, &TemplateTabWidget::matchCompleted,
                this, [](int count) {
                    Logger::instance()->info(
                        QString("匹配完成，找到 %1 个目标").arg(count));
                });
        QShortcut* sc = new QShortcut(Qt::Key_Escape, this);
        connect(sc, &QShortcut::activated, tab,
                &TemplateTabWidget::clearMatchResults);
    }
    else if (tabName == "处理") {
        auto* tab = qobject_cast<ProcessTabWidget*>(widget);
        connect(tab, &ProcessTabWidget::algorithmChanged,
                this, &MainWindow::processAndDisplay);
    }
    else if (tabName == "提取") {
        auto* tab = qobject_cast<ExtractTabWidget*>(widget);
        connect(tab, &ExtractTabWidget::extractionChanged,
                this, &MainWindow::processAndDisplay);
    }
    else if (tabName == "直线") {
        auto* tab = qobject_cast<LineDetectTabWidget*>(widget);
        connect(tab, &LineDetectTabWidget::requestDrawReferenceLine,
                this, [this]() { m_view->startReferenceLineDrawing(); });
        connect(tab, &LineDetectTabWidget::requestClearReferenceLine,
                this, [this]() {
                    m_view->clearReferenceLine();
                    processAndDisplay();
                });
        connect(m_view, &ImageView::referenceLineDrawn,
                tab, &LineDetectTabWidget::setReferenceLine);
    }
    else if (tabName == "目标检测") {
        auto* tab = qobject_cast<ObjectDetectionTabWidget*>(widget);
        connect(tab, &ObjectDetectionTabWidget::detectionConfigChanged,
                this, &MainWindow::processAndDisplay);
    }
}

