#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "logger.h"
#include <QFile>
#include <QDir>
#include <QTimer>
#include <QInputDialog>
#include <QDebug>
#include <QShortcut>

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

    m_processDebounceTimer =new QTimer(this);
    m_processDebounceTimer ->setSingleShot(true);
    m_processDebounceTimer ->setInterval(150);

    connect(m_processDebounceTimer, &QTimer::timeout,this,&MainWindow::processAndDisplay);

    setupUI();
    setupConnections();

    setupSystemMonitor();

    QString logPath = QCoreApplication::applicationDirPath() + "/../../logs/test.log";
    Logger::instance()->setLogFile(logPath);
    Logger::instance()->enableFileLog(true);

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

    

    // 初始化日志页面
    m_logPage = std::make_unique<LogPage>(this);
    ui->stackedWidget_MainWindow->addWidget(m_logPage.get());

    // 将 Logger 的输出连接到 LogPage
    connect(Logger::instance(), &Logger::logMessage,
            m_logPage.get(), &LogPage::appendLog);

    // 确保初始显示 Home 页面
    ui->stackedWidget_MainWindow->setCurrentIndex(0);

}

MainWindow::~MainWindow()
{
    m_isDestroying = true;
    delete ui;
}

void MainWindow::setupSystemMonitor()
{
    m_systemMonitor->setupWithLogging(ui->label_cpu, ui->label_memory, 1000);
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
    // ========== 设置显示模式 ==========
    setDisplayModeForCurrentTab();

    // ========== 执行 Pipeline ==========
    cv::Mat currentImage = m_roiManager.getCurrentImage();
    const PipelineContext& result = m_pipelineManager->execute(currentImage);

    // ========== 显示结果 ==========
    cv::Mat displayImage = result.getFinalDisplay();
    showImage(displayImage);

    //========== 更新判定Tab显示 ==========
    int count = result.currentRegions;
    if (m_judgeTabWidget) {
        m_judgeTabWidget->setCurrentRegionCount(count);
    }
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
    m_pipelineManager->setDisplayMode(DisplayConfig::Mode::MaskGreenWhite); break;
    
    case 7: // 直线检测Tab
    m_pipelineManager->setDisplayMode(DisplayConfig::Mode::LineDetect); break;
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
}

void MainWindow::onRoiSelected(const QRectF &roiImgRectF)
{
    if (!m_roiManager.handleRoiSelected(roiImgRectF, ui->statusbar)) {
        return;
    }

    processAndDisplay();
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
    ui->tabWidget->hide();
    ui->stackedWidget_MainWindow->setCurrentIndex(1);
}

void MainWindow::on_btn_Home_clicked()
{
    ui->tabWidget->show();
    ui->stackedWidget_MainWindow->setCurrentIndex(0);
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
    QString filePath = QFileDialog::getOpenFileName(this, 
        "导入配置", "", "JSON Files (*.json)");
    
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
        config.roiRect = QRectF(roi.x, roi.y, roi.width, roi.height);
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