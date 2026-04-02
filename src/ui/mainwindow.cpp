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
    
    // 初始化日志页面
    ui->page_log->initialize();
    
    // 连接日志页面的返回主页信号
    connect(ui->page_log, &LogPage::requestGoHome, this, [this]() {
        ui->stackedWidget_main->setCurrentIndex(0);
    });
    
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

    m_processTabWidget = std::make_unique<ProcessTabWidget>(
        m_pipelineManager, this);
    ui->tabWidget->addTab(m_processTabWidget.get(), "处理");
    connect(m_processTabWidget.get(), &ProcessTabWidget::algorithmChanged,
            this, &MainWindow::processAndDisplay);
    m_processTabWidget->initialize();

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
    
    // 初始化Controller对象
    m_roiUiController = new RoiUiController(m_multiRoiConfig, m_roiManager, m_view, ui->statusbar, this);
    m_detectionUiController = new DetectionUiController(m_multiRoiConfig, ui->tabWidget, this);
    m_configController = new ConfigController(m_pipelineManager, m_roiManager, this);
    
    // 设置Controller的Tab名称列表
    m_detectionUiController->setTabNames(m_tabNames);
    
    // 初始隐藏所有Tab，等待检测项选中时再显示对应的Tab
    for (int i = 0; i < ui->tabWidget->count(); ++i) {
        ui->tabWidget->setTabVisible(i, false);
    }
    
    // 设置ConfigController的Tab Widget
    m_configController->setTabWidgets(
        m_enhanceTabWidget.get(),
        m_filterTabWidget.get(),
        m_extractTabWidget.get(),
        m_judgeTabWidget.get(),
        m_processTabWidget.get()
    );
    
    // 初始化ROI UI Controller的TreeView
    m_roiUiController->setupTreeView(ui->treeView);
    
    
    // 连接Controller信号
    connect(m_roiUiController, &RoiUiController::roiChanged, this, &MainWindow::processAndDisplay);
    connect(m_detectionUiController, &DetectionUiController::detectionChanged, this, [this]() {
        m_roiUiController->refreshRoiTreeView();
    });
    connect(m_configController, &ConfigController::configApplied, this, &MainWindow::processAndDisplay);
    
    // 连接检测项选中信号到Tab切换逻辑
    connect(m_roiUiController, &RoiUiController::detectionItemSelected, 
            this, [this](const QString& roiId, const QString& detectionId) {
        // 获取ROI和检测项
        RoiConfig* roi = m_multiRoiConfig->getRoi(roiId);
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
        
        RoiConfig* roi = m_multiRoiConfig->getRoi(currentRoiId);
        if (!roi) {
            QMessageBox::warning(this, "警告", "未找到选中的ROI");
            return;
        }
        
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
            RoiConfig* roi = m_multiRoiConfig->getRoi(roiId);
            
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

