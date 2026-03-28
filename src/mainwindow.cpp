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

    // 初始化日志页面
    m_logPage = std::make_unique<LogPage>(this);
    m_logPage->setWindowTitle("日志");
    m_logPage->resize(800, 600);

    // 将 Logger 的输出连接到 LogPage
    connect(Logger::instance(), &Logger::logMessage,
            m_logPage.get(), &LogPage::appendLog);

    // 初始化多ROI管理
    initMultiRoiManagement();
    connectMultiRoiSignals();

}

MainWindow::~MainWindow()
{
    m_isDestroying = true;
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
    // 显示日志页面作为独立对话框
    if (m_logPage) {
        m_logPage->show();
        m_logPage->raise();
        m_logPage->activateWindow();
    }
}

void MainWindow::on_btn_Home_clicked()
{
    // 如果日志页面是作为对话框显示的，隐藏它
    if (m_logPage && m_logPage->isVisible()) {
        m_logPage->hide();
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

// ========== 多ROI管理方法实现 ==========

void MainWindow::initMultiRoiManagement()
{
    // 创建ROI列表组件
    m_roiListWidget = std::make_unique<RoiListWidget>(this);
    m_roiListWidget->setRoiConfig(&m_multiRoiConfig);
    
    // 创建检测项配置组件
    m_detectionConfigWidget = std::make_unique<DetectionConfigWidget>(this);
    
    // 将组件添加到UI布局中
    // 找到ROI面板的布局并添加ROI列表组件
    QVBoxLayout* roiPanelLayout = ui->widget_roiPanel->findChild<QVBoxLayout*>("verticalLayout_roiPanel");
    if (roiPanelLayout) {
        // 移除原有的treeWidget_roiList
        QTreeWidget* oldTreeWidget = ui->treeWidget_roiList;
        if (oldTreeWidget) {
            int index = roiPanelLayout->indexOf(oldTreeWidget);
            if (index >= 0) {
                roiPanelLayout->removeWidget(oldTreeWidget);
                oldTreeWidget->hide();
            }
        }
        
        // 在分割线后插入ROI列表组件
        QFrame* separator = ui->line_separator;
        int separatorIndex = roiPanelLayout->indexOf(separator);
        if (separatorIndex >= 0) {
            roiPanelLayout->insertWidget(separatorIndex + 1, m_roiListWidget.get());
        }
    }
    
    // 找到检测项滚动区域的布局并添加检测配置组件
    QVBoxLayout* detectionListLayout = ui->scrollArea_detectionList->findChild<QVBoxLayout*>("verticalLayout_detectionList");
    if (detectionListLayout) {
        // 移除原有的无检测项提示
        QLabel* oldLabel = ui->label_noDetection;
        if (oldLabel) {
            detectionListLayout->removeWidget(oldLabel);
            oldLabel->hide();
        }
        
        // 添加检测配置组件
        detectionListLayout->addWidget(m_detectionConfigWidget.get());
    }
    
    Logger::instance()->info("多ROI管理组件初始化完成");
}

void MainWindow::connectMultiRoiSignals()
{
    // 连接ROI列表组件信号
    if (m_roiListWidget) {
        connect(m_roiListWidget.get(), &RoiListWidget::roiAddRequested,
                this, &MainWindow::addNewRoi);
        connect(m_roiListWidget.get(), &RoiListWidget::roiDeleteRequested,
                this, &MainWindow::deleteRoi);
        connect(m_roiListWidget.get(), &RoiListWidget::roiSelectionChanged,
                this, &MainWindow::selectRoi);
        connect(m_roiListWidget.get(), &RoiListWidget::roiRenameRequested,
                this, &MainWindow::renameRoi);
        connect(m_roiListWidget.get(), &RoiListWidget::roiActiveChanged,
                this, &MainWindow::toggleRoiActive);
        connect(m_roiListWidget.get(), &RoiListWidget::drawRoiRequested,
                this, &MainWindow::on_btn_drawRoi_clicked);
    }
    
    // 连接检测项配置组件信号
    if (m_detectionConfigWidget) {
        connect(m_detectionConfigWidget.get(), &DetectionConfigWidget::detectionAddRequested,
                this, &MainWindow::addDetectionItem);
        connect(m_detectionConfigWidget.get(), &DetectionConfigWidget::detectionDeleteRequested,
                this, &MainWindow::deleteDetectionItem);
        connect(m_detectionConfigWidget.get(), &DetectionConfigWidget::detectionConfigChanged,
                this, &MainWindow::updateDetectionItem);
    }
    
    Logger::instance()->info("多ROI管理信号连接完成");
}

void MainWindow::updateImageWithMultipleRois()
{
    // 获取当前图像
    cv::Mat currentImage = m_roiManager.getCurrentImage();
    if (currentImage.empty()) {
        return;
    }
    
    // 在图像上绘制所有ROI
    cv::Mat displayImage = currentImage.clone();
    
    for (const auto& roi : m_multiRoiConfig.getRois()) {
        if (!roi.isActive) {
            continue;
        }
        
        // 转换ROI坐标
        cv::Rect cvRect(roi.roiRect.x(), roi.roiRect.y(), 
                       roi.roiRect.width(), roi.roiRect.height());
        
        // 绘制ROI边界框
        cv::Scalar color;
        if (roi.color == "#FF6B6B") {
            color = cv::Scalar(107, 107, 255); // 红色
        } else if (roi.color == "#4ECDC4") {
            color = cv::Scalar(196, 205, 78); // 青色
        } else if (roi.color == "#45B7D1") {
            color = cv::Scalar(209, 183, 69); // 蓝色
        } else {
            color = cv::Scalar(0, 255, 0); // 默认绿色
        }
        
        // 绘制矩形
        cv::rectangle(displayImage, cvRect, color, 2);
        
        // 绘制ROI名称
        cv::Point textPoint(cvRect.x, cvRect.y - 5);
        cv::putText(displayImage, roi.roiName.toStdString(), 
                   textPoint, cv::FONT_HERSHEY_SIMPLEX, 0.5, color, 1);
        
        // 绘制检测项
        int yOffset = 20;
        for (const auto& detection : roi.detectionItems) {
            if (detection.enabled) {
                cv::Point detectionPoint(cvRect.x + 5, cvRect.y + yOffset);
                cv::putText(displayImage, detection.itemName.toStdString(), 
                           detectionPoint, cv::FONT_HERSHEY_SIMPLEX, 0.3, color, 1);
                yOffset += 15;
            }
        }
    }
    
    // 显示图像
    showImage(displayImage);
}

void MainWindow::addNewRoi(const QString& roiName)
{
    // 创建新的ROI配置
    RoiConfig newRoi(roiName, QRectF(100, 100, 200, 200));
    
    // 添加到配置中
    m_multiRoiConfig.addRoi(newRoi);
    
    // 刷新ROI列表
    if (m_roiListWidget) {
        m_roiListWidget->refreshRoiList();
        m_roiListWidget->selectRoi(newRoi.roiId);
    }
    
    // 更新图像显示
    updateImageWithMultipleRois();
    
    Logger::instance()->info(QString("已添加新ROI: %1").arg(roiName));
}

void MainWindow::deleteRoi(const QString& roiId)
{
    // 从配置中删除ROI
    if (m_multiRoiConfig.removeRoi(roiId)) {
        // 刷新ROI列表
        if (m_roiListWidget) {
            m_roiListWidget->refreshRoiList();
        }
        
        // 清空检测项配置
        if (m_detectionConfigWidget) {
            m_detectionConfigWidget->clearCurrentConfig();
        }
        
        // 更新图像显示
        updateImageWithMultipleRois();
        
        Logger::instance()->info("已删除ROI");
    }
}

void MainWindow::selectRoi(const QString& roiId)
{
    // 获取ROI配置
    RoiConfig* roi = m_multiRoiConfig.getRoi(roiId);
    if (!roi) {
        return;
    }
    
    // 更新检测项配置显示
    if (m_detectionConfigWidget) {
        m_detectionConfigWidget->setCurrentRoi(roi);
    }
    
    // 更新图像显示
    updateImageWithMultipleRois();
    
    Logger::instance()->info(QString("已选中ROI: %1").arg(roi->roiName));
}

void MainWindow::renameRoi(const QString& roiId, const QString& newName)
{
    // 获取ROI配置
    RoiConfig* roi = m_multiRoiConfig.getRoi(roiId);
    if (!roi) {
        return;
    }
    
    // 更新名称
    roi->roiName = newName;
    
    // 刷新ROI列表
    if (m_roiListWidget) {
        m_roiListWidget->refreshRoiList();
    }
    
    // 更新图像显示
    updateImageWithMultipleRois();
    
    Logger::instance()->info(QString("已重命名ROI为: %1").arg(newName));
}

void MainWindow::toggleRoiActive(const QString& roiId, bool active)
{
    // 获取ROI配置
    RoiConfig* roi = m_multiRoiConfig.getRoi(roiId);
    if (!roi) {
        return;
    }
    
    // 更新激活状态
    roi->isActive = active;
    
    // 刷新ROI列表
    if (m_roiListWidget) {
        m_roiListWidget->refreshRoiList();
    }
    
    // 更新图像显示
    updateImageWithMultipleRois();
    
    Logger::instance()->info(QString("ROI %1 已%2").arg(roi->roiName).arg(active ? "激活" : "停用"));
}

void MainWindow::addDetectionItem(DetectionType type, const QString& name)
{
    // 获取当前选中的ROI
    QString selectedRoiId = m_roiListWidget ? m_roiListWidget->getSelectedRoiId() : QString();
    if (selectedRoiId.isEmpty()) {
        QMessageBox::information(this, "提示", "请先选择一个ROI！");
        return;
    }
    
    // 获取ROI配置
    RoiConfig* roi = m_multiRoiConfig.getRoi(selectedRoiId);
    if (!roi) {
        return;
    }
    
    // 创建新的检测项
    DetectionItem newItem(name, type);
    
    // 添加到ROI中
    roi->addDetectionItem(newItem);
    
    // 刷新检测项配置显示
    if (m_detectionConfigWidget) {
        m_detectionConfigWidget->refreshDetectionList();
    }
    
    // 更新图像显示
    updateImageWithMultipleRois();
    
    Logger::instance()->info(QString("已添加检测项: %1").arg(name));
}

void MainWindow::deleteDetectionItem(const QString& itemId)
{
    // 获取当前选中的ROI
    QString selectedRoiId = m_roiListWidget ? m_roiListWidget->getSelectedRoiId() : QString();
    if (selectedRoiId.isEmpty()) {
        return;
    }
    
    // 获取ROI配置
    RoiConfig* roi = m_multiRoiConfig.getRoi(selectedRoiId);
    if (!roi) {
        return;
    }
    
    // 从ROI中删除检测项
    if (roi->removeDetectionItem(itemId)) {
        // 刷新检测项配置显示
        if (m_detectionConfigWidget) {
            m_detectionConfigWidget->refreshDetectionList();
        }
        
        // 更新图像显示
        updateImageWithMultipleRois();
        
        Logger::instance()->info("已删除检测项");
    }
}

void MainWindow::updateDetectionItem(const QString& itemId)
{
    // 这里可以添加更新检测项配置的逻辑
    // 目前只是简单地更新图像显示
    updateImageWithMultipleRois();
    
    Logger::instance()->info("已更新检测项配置");
}

void MainWindow::saveMultiRoiConfig(const QString& filePath)
{
    // 保存多ROI配置到文件
    QJsonDocument doc = m_multiRoiConfig.toJsonDocument();
    QByteArray data = doc.toJson(QJsonDocument::Indented);
    
    QFile file(filePath);
    if (file.open(QIODevice::WriteOnly)) {
        file.write(data);
        file.close();
        Logger::instance()->info(QString("多ROI配置已保存到: %1").arg(filePath));
    } else {
        Logger::instance()->error(QString("无法保存多ROI配置到: %1").arg(filePath));
    }
}

void MainWindow::loadMultiRoiConfig(const QString& filePath)
{
    // 从文件加载多ROI配置
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        Logger::instance()->error(QString("无法加载多ROI配置从: %1").arg(filePath));
        return;
    }
    
    QByteArray data = file.readAll();
    file.close();
    
    QJsonDocument doc = QJsonDocument::fromJson(data);
    m_multiRoiConfig.fromJsonDocument(doc);
    
    // 刷新ROI列表
    if (m_roiListWidget) {
        m_roiListWidget->refreshRoiList();
    }
    
    // 清空检测项配置
    if (m_detectionConfigWidget) {
        m_detectionConfigWidget->clearCurrentConfig();
    }
    
    // 更新图像显示
    updateImageWithMultipleRois();
    
    Logger::instance()->info(QString("已从 %1 加载多ROI配置").arg(filePath));
}
