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

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , m_pipelineManager(new PipelineManager(this))
    , m_systemMonitor(new SystemMonitor(this))
    , m_fileManager(new FileManager(this))
    , m_processDebounceTimer(nullptr)
    , m_currentTabIndex(0)
    , m_multiRoiConfig(new MultiRoiConfig())
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
                // 更新UI显示
                cv::Mat displayImage = result.getFinalDisplay();
                showImage(displayImage);

                // 更新判定Tab显示
                int count = result.currentRegions;
                if (m_judgeTabWidget) {
                    m_judgeTabWidget->setCurrentRegionCount(count);
                }
                
                // 更新直线检测Tab匹配结果
                if (m_lineDetectTabWidget && result.totalLineCount > 0) {
                    const PipelineConfig& cfg = m_pipelineManager->getConfig();
                    m_lineDetectTabWidget->updateMatchResultStatus(
                        result.matchedLineCount, 
                        result.totalLineCount, 
                        cfg.angleThreshold, 
                        cfg.distanceThreshold
                    );
                }
                
                // 更新条码Tab识别结果
                if (m_barcodeTabWidget) {
                    m_barcodeTabWidget->updateResultsTable(result.barcodeResults);
                    m_barcodeTabWidget->updateStatus(result.barcodeStatus);
                }
                // 恢复状态栏
                ui->statusbar->showMessage("处理完成", 2000);
            });

    setupUI();
    setupConnections();

    setupSystemMonitor();

    // 设置日志文件路径（使用正确的相对路径）
    QString logDir = QCoreApplication::applicationDirPath() + "/logs";
    QDir dir(logDir);
    if (!dir.exists()) {
        dir.mkpath(".");
    }
    QString logPath = logDir + "/test.log";
    Logger::instance()->setLogFile(logPath);
    Logger::instance()->enableFileLog(true);
    
    // 将日志输出控件连接到Logger
    Logger::instance()->setTextEdit(ui->textEdit_log);
    
    // 创建 ImageTabWidget 并添加到 tabWidget
    m_imageTabWidget = std::make_unique<ImageTabWidget>(m_pipelineManager, this);
    ui->tabWidget->addTab(m_imageTabWidget.get(), "图像");
    connect(m_imageTabWidget.get(), &ImageTabWidget::channelChanged,
            this, [this](int channel) {
                
                m_pipelineManager->getConfig().channel = static_cast<PipelineConfig::Channel>(channel);
            
                processAndDisplay();
                
            });

    m_enhanceTabWidget = std::make_unique<EnhanceTabWidget>(
    m_pipelineManager, this);
    ui->tabWidget->addTab(m_enhanceTabWidget.get(), "增强");
    connect(m_enhanceTabWidget.get(), &EnhanceTabWidget::processRequested,
            this, &MainWindow::processAndDisplay);

    m_filterTabWidget = std::make_unique<FilterTabWidget>(m_pipelineManager, this);
    ui->tabWidget->addTab(m_filterTabWidget.get(), "过滤");
    connect(m_filterTabWidget.get(), &FilterTabWidget::filterConfigChanged,
            this, &MainWindow::processAndDisplay);

    m_templateTabWidget = std::make_unique<TemplateTabWidget>(
        m_view, &m_roiManager, this);
    ui->tabWidget->addTab(m_templateTabWidget.get(), "补正");
    m_templateTabWidget->initialize();
    connect(m_templateTabWidget.get(), &TemplateTabWidget::imageToShow,
            this, &MainWindow::showImage);
    connect(m_templateTabWidget.get(), &TemplateTabWidget::templateCreated,
            this, [this](const QString& name) {
                Logger::instance()->info(QString("模板已创建: %1").arg(name));
            });
    connect(m_templateTabWidget.get(), &TemplateTabWidget::matchCompleted,
            this, [](int count) {
                Logger::instance()->info(QString("匹配完成，找到 %1 个目标").arg(count));
            });

    // 添加快捷键清除匹配结果（按 Escape 键）
    QShortcut *clearMatchShortcut = new QShortcut(Qt::Key_Escape, this);
    connect(clearMatchShortcut, &QShortcut::activated, m_templateTabWidget.get(), &TemplateTabWidget::clearMatchResults);

    // m_algorithmController = std::make_unique<AlgorithmTabController>(
    //     ui, m_pipelineManager, this);
    // connect(m_algorithmController.get(), &AlgorithmTabController::algorithmChanged,
    //         this, &MainWindow::processAndDisplay);
    // m_algorithmController->initialize();

    m_processTabWidget = std::make_unique<ProcessTabWidget>(
        m_pipelineManager, this);
    ui->tabWidget->addTab(m_processTabWidget.get(), "处理");
    connect(m_processTabWidget.get(), &ProcessTabWidget::algorithmChanged,
            this, &MainWindow::processAndDisplay);
    m_processTabWidget->initialize();

    // m_extractController = std::make_unique<ExtractTabController>(
    //     ui, m_pipelineManager, m_view, &m_roiManager, this);
    // connect(m_extractController.get(), &ExtractTabController::extractionChanged,
    //         this, &MainWindow::processAndDisplay);
    // m_extractController->initialize();


    m_extractTabWidget = std::make_unique<ExtractTabWidget>(
        m_pipelineManager, m_view, &m_roiManager, this);
    ui->tabWidget->addTab(m_extractTabWidget.get(), "提取");
    m_extractTabWidget->initialize();
    connect(m_extractTabWidget.get(), &ExtractTabWidget::extractionChanged,
            this, &MainWindow::processAndDisplay);

    m_judgeTabWidget = std::make_unique<JudgeTabWidget>(m_pipelineManager, this);
    ui->tabWidget->addTab(m_judgeTabWidget.get(), "判定");

    m_lineDetectTabWidget = std::make_unique<LineDetectTabWidget>(
        m_pipelineManager, [this]() { m_processDebounceTimer->start(); }, this);
    ui->tabWidget->addTab(m_lineDetectTabWidget.get(), "直线");
    m_lineDetectTabWidget->initialize();
    
    // 连接参考线绘制信号
    connect(m_lineDetectTabWidget.get(), &LineDetectTabWidget::requestDrawReferenceLine,
            this, [this]() {
                // 启用参考线绘制模式
                m_view->startReferenceLineDrawing();
            });
    
    connect(m_lineDetectTabWidget.get(), &LineDetectTabWidget::requestClearReferenceLine,
            this, [this]() {
                // 清除参考线
                m_view->clearReferenceLine();
                // 重新处理图像以清除匹配结果
                processAndDisplay();
            });
    
    // 连接参考线绘制完成信号
    connect(m_view, &ImageView::referenceLineDrawn,
            m_lineDetectTabWidget.get(), &LineDetectTabWidget::setReferenceLine);

    // 创建条码Tab并添加到tabWidget
    m_barcodeTabWidget = std::make_unique<BarcodeTabWidget>(
        m_pipelineManager, [this]() { m_processDebounceTimer->start(); }, this);
    ui->tabWidget->addTab(m_barcodeTabWidget.get(), "条码");

    // 初始化检测项管理器和TreeView（在UI初始化之后）
    setupDetectionManager();
    setupTreeView();
    
    // 记录所有Tab Widget和名称，用于动态切换
    m_tabWidgets = {
        m_imageTabWidget.get(),
        m_enhanceTabWidget.get(),
        m_filterTabWidget.get(),
        m_templateTabWidget.get(),
        m_processTabWidget.get(),
        m_extractTabWidget.get(),
        m_judgeTabWidget.get(),
        m_lineDetectTabWidget.get(),
        m_barcodeTabWidget.get()
    };
    m_tabNames = {"图像", "增强", "过滤", "补正", "处理", "提取", "判定", "直线", "条码"};

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
}

void MainWindow::setupConnections()
{
    // 按钮信号由Qt自动连接（通过on_<objectName>_<signal>命名规则）
    // 无需手动连接以下按钮：
    // - btn_Home
    // - btn_Log  
    // - btn_openImg
    // - btn_saveImg
    // - btn_drawRoi
    // - btn_resetROI
    // - btn_saveConfig
    // - btn_importConfig
    // connect(ui->btn_addDetect, &QPushButton::clicked, this, [this]() {
    //     // 添加检测功能（如果需要实现）
    //     Logger::instance()->info("添加检测按钮被点击");
    // });
    // connect(ui->btn_deleteDetect, &QPushButton::clicked, this, [this]() {
    //     // 删除检测功能（如果需要实现）
    //     Logger::instance()->info("删除检测按钮被点击");
    // });
    
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

    connect(m_view, &ImageView::roiSelected,
            this, &MainWindow::onRoiSelected);

    connect(m_view, &ImageView::polygonDrawingPointAdded, this,
            [](const QString& type, const QPointF& point) {
                Logger::instance()->info(
                    QString("[%1] 添加顶点: (%2, %3)")
                        .arg(type)
                        .arg(point.x(), 0, 'f', 1)
                        .arg(point.y(), 0, 'f', 1)
                    );
            });
    // connect(ui->btn_saveConfig, &QPushButton::clicked, this, &MainWindow::on_btn_saveConfig_clicked);
    // connect(ui->btn_importConfig, &QPushButton::clicked, this, &MainWindow::on_btn_importConfig_clicked);

}

// ========== 核心处理 ==========

void MainWindow::processAndDisplay()
{
    // 如果正在处理，跳过本次请求（避免堆积）
    if (m_isProcessing) {
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
    ui->statusbar->showMessage("正在处理...");

    QFuture<PipelineContext> future = QtConcurrent::run(
        [this, currentImage]() -> PipelineContext {
            return m_pipelineManager->execute(currentImage);
        }
    );
    m_pipelineWatcher.setFuture(future);
}

void MainWindow::setDisplayModeForCurrentTab()
{
    switch (m_currentTabIndex)
    {
    case 0: // 图像Tab
    m_pipelineManager->setDisplayMode(DisplayConfig::Mode::Channel); break;
    case 1: // 增强Tab
    m_pipelineManager->setDisplayMode(DisplayConfig::Mode::Enhanced); break;
    case 2: // 过滤Tab
    m_pipelineManager->setDisplayMode(DisplayConfig::Mode::MaskGreenWhite); break;
    case 3: // 补正Tab
    m_pipelineManager->setDisplayMode(DisplayConfig::Mode::Original); break;
    case 4: // 处理Tab
    m_pipelineManager->setDisplayMode(DisplayConfig::Mode::Processed); break;
    case 5: // 提取Tab
    m_pipelineManager->setDisplayMode(DisplayConfig::Mode::MaskGreenWhite); break;

    case 6: // 判定Tab
    m_pipelineManager->setDisplayMode(DisplayConfig::Mode::MaskOverlay); break;
    
    case 7: // 直线检测Tab
    m_pipelineManager->setDisplayMode(DisplayConfig::Mode::LineDetect); break;
    case 8: // 条码Tab
    m_pipelineManager->setDisplayMode(DisplayConfig::Mode::Original); break;
    default: m_pipelineManager->setDisplayMode(DisplayConfig::Mode::Original); break;
    }
}

void MainWindow::showImage(const cv::Mat &img)
{
    QImage qimg = ImageUtils::Mat2Qimage(img);
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

    QMetaObject::Connection conn = connect(m_fileManager, &FileManager::imageLoaded,
            this, [this](const cv::Mat&) {
                processAndDisplay();
            });

    m_fileManager->readImageFile(fileName);
    disconnect(conn);
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
    RoiConfig* roi = m_multiRoiConfig->getRoi(roiId);
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
        m_multiRoiConfig->removeRoi(roiId);
        
        // 刷新树形视图
        refreshRoiTreeView();
        
        // 重置ROI管理器
        m_roiManager.resetRoiWithUI(m_view, ui->statusbar);
        
        // 重新处理图像
        processAndDisplay();
        
        Logger::instance()->info(QString("已删除ROI: %1").arg(roi->roiName));
    }
}

//清空当前日志
void MainWindow::on_btn_clearLog_clicked()
{
    Logger::instance()->clear();
    Logger::instance()->info("日志已清空");
}

void MainWindow::on_btn_openLog_clicked()
{
    Logger::instance()->openLogFolder(true);
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
    
    // 显示已有日志（不重新连接信号，避免断开构造函数中的连接）
    if (ui->textEdit_log) {
        // 清空现有内容
        ui->textEdit_log->clear();
        
        // 显示已有日志
        QStringList logs = Logger::instance()->getRecentLogs(100);
        for (const QString& log : logs) {
            ui->textEdit_log->append(log);
        }
    }
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
    QString filePath = QFileDialog::getSaveFileName(this, 
        "保存配置", "", "JSON Files (*.json)");
    if (filePath.isEmpty()) return;
    AppConfig config;
    collectConfigFromUI(config);

    QString configPath = ConfigManager::instance().getDefaultConfigPath();
    if (ConfigManager::instance().saveConfig(config, filePath)) {
        Logger::instance()->info("配置已保存到: " + filePath);
    }
}

void MainWindow::loadConfig()
{
    QString configPath = ConfigManager::instance().getDefaultConfigPath();
    QString filePath = QFileDialog::getOpenFileName(this,
        "导入配置", configPath, "JSON Files (*.json)");

    if (filePath.isEmpty()) return;
    
    AppConfig config;
    if (ConfigManager::instance().loadConfig(config, filePath)) {
        applyConfigToUI(config);
        Logger::instance()->info("配置已从: " + filePath + " 加载");
    }
}


void MainWindow::collectConfigFromUI(AppConfig& config)
{
    // 收集 ROI 配置
    if (m_roiManager.isRoiActive()) {
        cv::Rect roi = m_roiManager.getLastRoi();
        config.roiRect = QRectF(static_cast<qreal>(roi.x), 
                               static_cast<qreal>(roi.y), 
                               static_cast<qreal>(roi.width), 
                               static_cast<qreal>(roi.height));
    }

    // 收集增强参数
    if (m_enhanceTabWidget) {
        m_enhanceTabWidget->getEnhanceConfig(config.brightness, config.contrast,
                                             config.gamma, config.sharpen);
    }

    // 收集过滤配置
    if (m_filterTabWidget) {
        m_filterTabWidget->getFilterConfig(config.filterMode, config.grayLow, config.grayHigh);
    }

    // 收集提取配置
    if (m_extractTabWidget) {
        m_extractTabWidget->getExtractConfig(config.shapeFilterConfig);
    }

    // 收集判定配置
    if (m_judgeTabWidget)
    {
        m_judgeTabWidget->getJudgeConfig(config.minRegionCount, config.maxRegionCount, config.currentRegionCount);
    }

    // 收集算法队列
    config.algorithmQueue = m_pipelineManager->getAlgorithmQueue();

}

void MainWindow::applyConfigToUI(const AppConfig& config)
{
    // 应用 ROI 配置
    if (config.roiRect.isValid()) {
        m_roiManager.applyRoi(config.roiRect);
        Logger::instance()->info("已恢复 ROI 配置");
    }

    // 应用增强参数
    if (m_enhanceTabWidget) {
        m_enhanceTabWidget->setEnhanceConfig(config.brightness, config.contrast,
                                             config.gamma, config.sharpen);
    }

    // 应用过滤配置
    if (m_filterTabWidget) {
        m_filterTabWidget->setFilterConfig(config.filterMode, config.grayLow, config.grayHigh);
    }

    // 应用提取配置
    if (m_extractTabWidget) {
        m_extractTabWidget->setExtractConfig(config.shapeFilterConfig);
    }

    // 应用判定配置
    if (m_judgeTabWidget)
    {
        m_judgeTabWidget->setJudgeConfig(config.minRegionCount, config.maxRegionCount, config.currentRegionCount);
    }

    // 应用算法队列
    m_pipelineManager->clearAlgorithmQueue();
    for (const auto& step : config.algorithmQueue)
    {
        m_pipelineManager->addAlgorithmStep(step);
    }

    // 刷新ProcessTabWidget的UI列表显示
    if (m_processTabWidget)
    {
        m_processTabWidget->refreshAlgorithmListUI(config.algorithmQueue);
    }

    // 同步Pipeline中的提取配置
    m_pipelineManager->setShapeFilterConfig(config.shapeFilterConfig);

    // 触发Pipeline更新以应用所有配置
    processAndDisplay();
}

// ========== 检测项管理 ==========

void MainWindow::setupDetectionManager()
{
    m_detectionManager = new DetectionManager(this);
    
    // 连接信号
    connect(m_detectionManager, &DetectionManager::detectionItemAdded,
            this, [this](int id) {
                updateTreeView();
                Logger::instance()->info(QString("检测项已添加: ID=%1").arg(id));
            });
    
    connect(m_detectionManager, &DetectionManager::detectionItemRemoved,
            this, [this](int id) {
                updateTreeView();
                Logger::instance()->info(QString("检测项已删除: ID=%1").arg(id));
            });
    
    connect(m_detectionManager, &DetectionManager::currentSelectionChanged,
            this, [this](int oldId, int newId) {
                if (newId != -1) {
                    TabConfig config = m_detectionManager->getTabConfig(newId);
                    switchToTabConfig(config);
                    Logger::instance()->info(QString("切换到检测项: ID=%1").arg(newId));
                }
            });
    
    // 连接添加检测项按钮
    connect(ui->btn_addDetection, &QPushButton::clicked,
            this, &MainWindow::onAddDetectionClicked);
    
    // 连接删除检测项按钮
    connect(ui->btn_deleteDetection, &QPushButton::clicked,
            this, &MainWindow::onDeleteDetectionClicked);
}

void MainWindow::setupTreeView()
{
    // 设置树形控件属性
    ui->treeView->setHeaderHidden(true);
    ui->treeView->setEditTriggers(QAbstractItemView::NoEditTriggers);
    ui->treeView->setContextMenuPolicy(Qt::CustomContextMenu);
    
    // 连接信号
    connect(ui->treeView, &QTreeWidget::itemClicked,
            this, &MainWindow::onRoiTreeItemClicked);
    connect(ui->treeView, &QTreeWidget::itemDoubleClicked,
            this, &MainWindow::onRoiTreeItemDoubleClicked);
    connect(ui->treeView, &QTreeWidget::customContextMenuRequested,
            this, &MainWindow::onRoiTreeContextMenuRequested);
    
    // 连接绘制ROI完成信号
    connect(m_view, &ImageView::roiSelected, this, [this](const QRectF& rect) {
        // 创建新的ROI
        QString roiName = QString("ROI_%1").arg(m_multiRoiConfig->size() + 1);
        RoiConfig roi(roiName, rect);
        m_multiRoiConfig->addRoi(roi);
        
        // 刷新列表
        refreshRoiTreeView();
        
        Logger::instance()->info(QString("已添加ROI: %1").arg(roiName));
    });
    
    // 刷新显示
    refreshRoiTreeView();
}

void MainWindow::onAddDetectionClicked()
{
    qDebug() << "[DEBUG] onAddDetectionClicked: m_currentSelectedRoiId =" << m_currentSelectedRoiId;
    
    // 检查是否已加载图像
    if (m_roiManager.getFullImage().empty()) {
        QMessageBox::warning(this, "警告", "请先加载一张图像");
        return;
    }
    
    // 检查是否有选中的ROI（使用 m_currentSelectedRoiId）
    if (m_currentSelectedRoiId.isEmpty()) {
        // 如果没有选中的ROI，检查是否有任何ROI
        if (m_multiRoiConfig->getRois().isEmpty()) {
            QMessageBox::warning(this, "警告", "请先绘制一个ROI");
            return;
        }
        QMessageBox::warning(this, "警告", "请先在左侧列表中选择一个ROI");
        return;
    }
    
    // 获取ROI配置
    RoiConfig* roi = m_multiRoiConfig->getRoi(m_currentSelectedRoiId);
    if (!roi) {
        qDebug() << "[DEBUG] onAddDetectionClicked: ROI not found for ID =" << m_currentSelectedRoiId;
        QMessageBox::warning(this, "警告", "未找到选中的ROI");
        return;
    }
    
    qDebug() << "[DEBUG] onAddDetectionClicked: Found ROI =" << roi->roiName << ", ID =" << roi->roiId;
    
    // 创建对话框
    QDialog dialog(this);
    dialog.setWindowTitle("添加检测项");
    dialog.setMinimumWidth(250);
    dialog.setWindowFlags(dialog.windowFlags() & ~Qt::WindowContextHelpButtonHint);
    
    // 创建布局
    QVBoxLayout* layout = new QVBoxLayout(&dialog);
    layout->setContentsMargins(20, 20, 20, 20);
    layout->setSpacing(15);
    
    // 添加标签
    QLabel* label = new QLabel("请选择检测项类型:", &dialog);
    layout->addWidget(label);
    
    // 创建ComboBox
    QComboBox* comboBox = new QComboBox(&dialog);
    comboBox->addItem("Blob分析", static_cast<int>(DetectionType::Blob));
    comboBox->addItem("直线检测", static_cast<int>(DetectionType::Line));
    comboBox->addItem("条码识别", static_cast<int>(DetectionType::Barcode));
    comboBox->setMinimumWidth(150);
    layout->addWidget(comboBox);
    
    // 添加按钮
    QDialogButtonBox* buttonBox = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    layout->addWidget(buttonBox);
    
    // 连接按钮信号
    connect(buttonBox, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    
    // 显示对话框（自动居中）
    if (dialog.exec() == QDialog::Accepted) {
        int index = comboBox->currentIndex();
        if (index >= 0) {
            DetectionType type = static_cast<DetectionType>(comboBox->currentData().toInt());
            
            // 创建检测项
            QString typeName = detectionTypeToString(type);
            DetectionItem item(typeName, type);
            
            qDebug() << "[DEBUG] onAddDetectionClicked: Adding detection item to ROI =" << roi->roiName;
            
            // 添加到ROI
            roi->addDetectionItem(item);
            
            // 刷新树形视图
            refreshRoiTreeView();
            
            Logger::instance()->info(QString("已为ROI %1 添加检测项: %2").arg(roi->roiName).arg(typeName));
        }
    }
}

void MainWindow::onDetectionItemClicked(const QModelIndex& index)
{
    if (!index.isValid()) {
        return;
    }
    
    // 获取检测项ID（存储在UserRole中）
    int id = index.data(Qt::UserRole).toInt();
    
    // 设置当前选中
    m_detectionManager->setCurrentSelectedId(id);
    
    // 高亮选中的项
    ui->treeView->setCurrentIndex(index);
}

void MainWindow::onDeleteDetectionClicked()
{
    int currentId = m_detectionManager->getCurrentSelectedId();
    if (currentId == -1) {
        QMessageBox::warning(this, "警告", "请先选择要删除的检测项");
        return;
    }
    
    DetectionItem* item = m_detectionManager->getDetectionItem(currentId);
    if (!item) {
        return;
    }
    
    // 确认删除
    QMessageBox::StandardButton reply = QMessageBox::question(
        this, "确认删除",
        QString("确定要删除检测项 \"%1\" 吗？").arg(item->itemName),
        QMessageBox::Yes | QMessageBox::No
    );
    
    if (reply == QMessageBox::Yes) {
        m_detectionManager->removeDetectionItem(currentId);
    }
}

void MainWindow::updateTreeView()
{
    // 清空树形控件
    ui->treeView->clear();
    
    const QList<DetectionItem>& items = m_detectionManager->getAllDetectionItems();
    int currentId = m_detectionManager->getCurrentSelectedId();
    
    for (int i = 0; i < items.size(); ++i) 
    {
        const DetectionItem& item = items[i];
        
        // 创建树形节点
        QTreeWidgetItem* treeItem = new QTreeWidgetItem(ui->treeView);
        
        // 设置显示文本
        QString displayText = QString("%1 [%2]").arg(item.itemName).arg(detectionTypeToString(item.type));
        treeItem->setText(0, displayText);
        
        // 存储索引作为ID（使用索引+1作为唯一标识）
        treeItem->setData(0, Qt::UserRole, i + 1);
        
        // 设置图标
        switch (item.type) {
            case DetectionType::Blob:
                treeItem->setIcon(0, QIcon(":/icons/blob.png"));
                break;
            case DetectionType::Line:
                treeItem->setIcon(0, QIcon(":/icons/line.png"));
                break;
            case DetectionType::Barcode:
                treeItem->setIcon(0, QIcon(":/icons/barcode.png"));
                break;
            default:
                break;
        }
        
        // 如果是当前选中的项，设置高亮
        if (i + 1 == currentId) {
            treeItem->setSelected(true);
        }
    }
    
    // 展开所有项
    ui->treeView->expandAll();
}

void MainWindow::switchToTabConfig(const TabConfig& config)
{
    // 隐藏所有Tab
    for (int i = 0; i < ui->tabWidget->count(); ++i) {
        ui->tabWidget->setTabVisible(i, false);
    }
    
    // 显示配置中指定的Tab
    for (const QString& tabName : config.tabNames) {
        for (int i = 0; i < m_tabNames.size(); ++i) {
            if (m_tabNames[i] == tabName) {
                ui->tabWidget->setTabVisible(i, true);
                break;
            }
        }
    }
    
    // 切换到第一个可见的Tab
    for (int i = 0; i < ui->tabWidget->count(); ++i) {
        if (ui->tabWidget->isTabVisible(i)) {
            ui->tabWidget->setCurrentIndex(i);
            m_currentTabIndex = i;
            break;
        }
    }
}


// ========== ROI树形视图管理 ==========

void MainWindow::refreshRoiTreeView()
{
    // 保存当前选中的ROI ID，以便刷新后恢复选中状态
    QString previouslySelectedRoiId = m_currentSelectedRoiId;
    
    qDebug() << "[DEBUG] refreshRoiTreeView: previouslySelectedRoiId =" << previouslySelectedRoiId;
    
    ui->treeView->clear();
    
    const QList<RoiConfig>& rois = m_multiRoiConfig->getRois();
    
    qDebug() << "[DEBUG] refreshRoiTreeView: Total ROI count =" << rois.size();
    
    QTreeWidgetItem* itemToSelect = nullptr;  // 要选中的项目
    
    for (const auto& roi : rois) {
        // 创建ROI节点
        QTreeWidgetItem* roiItem = new QTreeWidgetItem(ui->treeView);
        
        // 显示格式：ROI名称 [检测项类型]
        QString detectionTypes;
        for (int i = 0; i < roi.detectionItems.size(); ++i) {
            if (i > 0) detectionTypes += ", ";
            detectionTypes += detectionTypeToString(roi.detectionItems[i].type);
        }
        
        if (detectionTypes.isEmpty()) {
            roiItem->setText(0, roi.roiName);
        } else {
            roiItem->setText(0, QString("%1 [%2]").arg(roi.roiName).arg(detectionTypes));
        }
        
        roiItem->setData(0, Qt::UserRole, roi.roiId);
        roiItem->setFlags(roiItem->flags() | Qt::ItemIsSelectable | Qt::ItemIsEnabled);
        
        // 设置ROI颜色
        QColor roiColor(roi.color);
        roiItem->setForeground(0, QBrush(roiColor));
        
        // 记录需要选中的项目
        if (roi.roiId == previouslySelectedRoiId) {
            itemToSelect = roiItem;
            qDebug() << "[DEBUG] refreshRoiTreeView: Found matching ROI to select =" << roi.roiName << ", ID =" << roi.roiId;
        }
        
        // 添加检测项子节点（简化显示）
        for (const auto& detection : roi.detectionItems) {
            QTreeWidgetItem* detectionItem = new QTreeWidgetItem(roiItem);
            detectionItem->setText(0, detection.itemName);
            detectionItem->setData(0, Qt::UserRole, detection.itemId);
            detectionItem->setData(0, Qt::UserRole + 1, "detection"); // 标记为检测项
            
            // 根据检测类型设置颜色
            QColor detectionColor;
            switch (detection.type) {
                case DetectionType::Blob:
                    detectionColor = QColor("#4CAF50");
                    break;
                case DetectionType::Line:
                    detectionColor = QColor("#2196F3");
                    break;
                case DetectionType::Barcode:
                    detectionColor = QColor("#FF9800");
                    break;
                default:
                    detectionColor = QColor("#9E9E9E");
                    break;
            }
            detectionItem->setForeground(0, QBrush(detectionColor));
        }
    }
    
    // 展开所有项
    ui->treeView->expandAll();
    
    // 恢复选中状态
    if (itemToSelect) {
        // 如果之前选中的ROI还存在，恢复选中状态
        ui->treeView->setCurrentItem(itemToSelect);
        // 确保m_currentSelectedRoiId被正确设置
        m_currentSelectedRoiId = itemToSelect->data(0, Qt::UserRole).toString();
        qDebug() << "[DEBUG] refreshRoiTreeView: Restored selection to previously selected ROI, ID =" << m_currentSelectedRoiId;
    } else if (ui->treeView->topLevelItemCount() > 0) {
        // 如果之前选中的ROI不存在了（被删除了），选中最后一个ROI
        QTreeWidgetItem* lastItem = ui->treeView->topLevelItem(
            ui->treeView->topLevelItemCount() - 1
        );
        ui->treeView->setCurrentItem(lastItem);
        m_currentSelectedRoiId = lastItem->data(0, Qt::UserRole).toString();
        qDebug() << "[DEBUG] refreshRoiTreeView: Previously selected ROI not found, selected last ROI =" << m_currentSelectedRoiId;
    } else {
        // 没有任何ROI
        m_currentSelectedRoiId.clear();
        qDebug() << "[DEBUG] refreshRoiTreeView: No ROI exists, cleared selection";
    }
    
    qDebug() << "[DEBUG] refreshRoiTreeView: Final m_currentSelectedRoiId =" << m_currentSelectedRoiId;
}

void MainWindow::onRoiTreeItemClicked(QTreeWidgetItem* item, int column)
{
    Q_UNUSED(column);
    
    if (!item) {
        qDebug() << "[DEBUG] onRoiTreeItemClicked: item is null";
        return;
    }
    
    QString itemId = item->data(0, Qt::UserRole).toString();
    QString itemType = item->data(0, Qt::UserRole + 1).toString();
    
    qDebug() << "[DEBUG] onRoiTreeItemClicked: itemId =" << itemId << ", itemType =" << itemType;
    qDebug() << "[DEBUG] onRoiTreeItemClicked: m_currentSelectedRoiId before update =" << m_currentSelectedRoiId;
    
    if (itemType == "detection") {
        // 点击的是检测项，找到父ROI
        QTreeWidgetItem* parentItem = item->parent();
        if (parentItem) {
            m_currentSelectedRoiId = parentItem->data(0, Qt::UserRole).toString();
            qDebug() << "[DEBUG] onRoiTreeItemClicked: Detection item clicked, parent ROI ID =" << m_currentSelectedRoiId;
        } else {
            qDebug() << "[DEBUG] onRoiTreeItemClicked: Detection item has no parent!";
        }
        
        // 获取检测项类型并切换Tab
        RoiConfig* roi = m_multiRoiConfig->getRoi(m_currentSelectedRoiId);
        if (roi) {
            // 通过检测项名称查找对应的检测类型
            QString detectionName = item->text(0);
            for (const auto& detection : roi->detectionItems) {
                if (detection.itemName == detectionName) {
                    TabConfig config = TabConfig::getConfigForType(detection.type);
                    switchToTabConfig(config);
                    break;
                }
            }
        }
        
        Logger::instance()->info(QString("选中检测项: %1").arg(item->text(0)));
    } else {
        // 点击的是ROI
        m_currentSelectedRoiId = itemId;
        qDebug() << "[DEBUG] onRoiTreeItemClicked: ROI item clicked, m_currentSelectedRoiId =" << m_currentSelectedRoiId;
        
        // 在图像视图中显示对应的ROI
        RoiConfig* roi = m_multiRoiConfig->getRoi(itemId);
        if (roi) {
            // 清除当前绘制的ROI
            m_view->clearRoi();
            
            // 重置图像缩放到原始比例
            m_view->resetZoom();
            
            // 重新绘制选中的ROI
            m_view->setImage(ImageUtils::Mat2Qimage(m_roiManager.getFullImage()));
            
            // 在ROI管理器中应用选中的ROI区域
            // 将QRectF转换为cv::Rect，然后应用到ROI管理器
            cv::Rect roiRect(
                static_cast<int>(roi->roiRect.x()),
                static_cast<int>(roi->roiRect.y()),
                static_cast<int>(roi->roiRect.width()),
                static_cast<int>(roi->roiRect.height())
            );
            
            // 更新ROI管理器的当前图像
            // 注意：applyRoi期望QRectF类型，需要转换
            QRectF qroiRect(static_cast<qreal>(roiRect.x), 
                           static_cast<qreal>(roiRect.y), 
                           static_cast<qreal>(roiRect.width), 
                           static_cast<qreal>(roiRect.height));
            m_roiManager.applyRoi(qroiRect);
            // 重新处理图像以显示ROI区域
            processAndDisplay();
            
            Logger::instance()->info(QString("已显示ROI: %1").arg(roi->roiName));
        } else {
            Logger::instance()->info(QString("选中ROI: %1").arg(itemId));
        }
    }
    
    qDebug() << "[DEBUG] onRoiTreeItemClicked: m_currentSelectedRoiId after update =" << m_currentSelectedRoiId;
}

void MainWindow::onRoiTreeItemDoubleClicked(QTreeWidgetItem* item, int column)
{
    Q_UNUSED(column);
    
    if (!item) {
        return;
    }
    
    // 双击暂不实现任何功能
    // 用户可以后续根据需要添加
}

void MainWindow::onRoiTreeContextMenuRequested(const QPoint& pos)
{
    QTreeWidgetItem* item = ui->treeView->itemAt(pos);
    
    QMenu contextMenu(this);
    
    if (item) {
        QString itemType = item->data(0, Qt::UserRole + 1).toString();
        
        if (itemType == "detection") {
            // 检测项右键菜单
            QAction* deleteAction = contextMenu.addAction("删除检测项");
            connect(deleteAction, &QAction::triggered, [this, item]() {
                QString detectionId = item->data(0, Qt::UserRole).toString();
                QTreeWidgetItem* parentItem = item->parent();
                if (parentItem) {
                    QString roiId = parentItem->data(0, Qt::UserRole).toString();
                    RoiConfig* roi = m_multiRoiConfig->getRoi(roiId);
                    if (roi) {
                        roi->removeDetectionItem(detectionId);
                        refreshRoiTreeView();
                        Logger::instance()->info("已删除检测项");
                    }
                }
            });
        } else {
            // ROI右键菜单
            QAction* deleteAction = contextMenu.addAction("删除ROI");
            connect(deleteAction, &QAction::triggered, [this, item]() {
                QString roiId = item->data(0, Qt::UserRole).toString();
                if (QMessageBox::question(this, "确认删除", "确定要删除该ROI吗？") == QMessageBox::Yes) {
                    m_multiRoiConfig->removeRoi(roiId);
                    refreshRoiTreeView();
                    Logger::instance()->info("已删除ROI");
                }
            });
        }
    } else {
        // 空白区域右键菜单
        QAction* addAction = contextMenu.addAction("添加ROI");
        connect(addAction, &QAction::triggered, [this]() {
            // 提示用户绘制ROI
            Logger::instance()->info("请在图像上绘制ROI");
            m_roiManager.enableDrawRoiMode(m_view, ui->statusbar);
        });
    }
    
    contextMenu.exec(ui->treeView->viewport()->mapToGlobal(pos));
}

