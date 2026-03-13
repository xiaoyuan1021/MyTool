#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "logger.h"
#include <QFile>
#include <QDir>
#include <QTimer>
#include <QInputDialog>
#include <QDebug>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , m_pipelineManager(new PipelineManager(this))
    , m_systemMonitor(new SystemMonitor(this))
    , m_fileManager(new FileManager(this))
    , m_processDebounceTimer(nullptr)
    , m_currentTabIndex(0)
    , m_isDrawingRegion(false)
    , m_polygonItem(nullptr)
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
    ui->tabWidget->insertTab(0, m_imageTabWidget.get(), "图像");
    connect(m_imageTabWidget.get(), &ImageTabWidget::channelChanged,
            this, [this](int channel) {
                
                m_pipelineManager->getConfig().channel = static_cast<PipelineConfig::Channel>(channel);
            
                processAndDisplay();
                
            });

    m_enhanceTabWidget = std::make_unique<EnhanceTabWidget>(
    m_pipelineManager, this);
    ui->tabWidget->insertTab(1, m_enhanceTabWidget.get(), "增强");
    connect(m_enhanceTabWidget.get(), &EnhanceTabWidget::processRequested,
            this, &MainWindow::processAndDisplay);

    m_filterTabWidget = std::make_unique<FilterTabWidget>(m_pipelineManager, this);
    ui->tabWidget->insertTab(2, m_filterTabWidget.get(), "过滤");
    connect(m_filterTabWidget.get(), &FilterTabWidget::filterConfigChanged,
            this, &MainWindow::processAndDisplay);

    m_templateController = std::make_unique<TemplateController>(
        ui, m_view, &m_roiManager, this);
    m_templateController->initialize();
    connect(m_templateController.get(), &TemplateController::imageToShow,
            this, &MainWindow::showImage);
    connect(m_templateController.get(), &TemplateController::templateCreated,
            this, [this](const QString& name) {
                Logger::instance()->info(QString("模板已创建: %1").arg(name));
            });
    connect(m_templateController.get(), &TemplateController::matchCompleted,
            this, [](int count) {
                Logger::instance()->info(QString("匹配完成，找到 %1 个目标").arg(count));
            });

    m_algorithmController = std::make_unique<AlgorithmTabController>(
        ui, m_pipelineManager, this);
    connect(m_algorithmController.get(), &AlgorithmTabController::algorithmChanged,
            this, &MainWindow::processAndDisplay);
    m_algorithmController->initialize();

    m_extractController = std::make_unique<ExtractTabController>(
        ui, m_pipelineManager, m_view, &m_roiManager, this);
    connect(m_extractController.get(), &ExtractTabController::extractionChanged,
            this, &MainWindow::processAndDisplay);
    m_extractController->initialize();

    m_lineDetectController = std::make_unique<LineDetectTabController>
    (
        ui, m_pipelineManager, [this]() { m_processDebounceTimer->start(); }, this
    );
    m_lineDetectController->initialize();

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

    // 断开所有信号槽连接，防止 UI 销毁时触发槽函数
    if (m_imageTabWidget) {
        disconnect(m_imageTabWidget.get(), nullptr, this, nullptr);
    }
    if (m_enhanceTabWidget) {
        disconnect(m_enhanceTabWidget.get(), nullptr, this, nullptr);
    }
    if (m_filterTabWidget) {
        disconnect(m_filterTabWidget.get(), nullptr, this, nullptr);
    }
    if (m_lineDetectController) {
        disconnect(m_lineDetectController.get(), nullptr, this, nullptr);
    }

    // 断开 tabWidget 的信号
    if (ui && ui->tabWidget) {
        disconnect(ui->tabWidget, nullptr, this, nullptr);
    }

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

    //算法处理
    ui->doubleSpinBox_radius->setValue(3.5);
    ui->doubleSpinBox_radius->setDecimals(1);
    ui->spinBox_width ->setValue(5);
    ui->spinBox_height->setValue(5);

    ui->lineEdit_nowRegions->setReadOnly(true);
    ui->doubleSpinBox_minScore->setValue(0.5);
    ui->doubleSpinBox_minScore->setSingleStep(0.1);
    ui->spinBox_matchNumber->setValue(3);

    // 设置滑块-数字框对
    // setupSliderSpinBoxPair(ui->Slider_grayLow, ui->spinBox_grayLow, 0, 255, 0);
    // setupSliderSpinBoxPair(ui->Slider_grayHigh, ui->spinBox_grayHigh, 0, 255, 0);

    // setupSliderSpinBoxPair(ui->Slider_rgb_R_Low,ui->spinBox_rgb_R_Low,0,255,0);
    // setupSliderSpinBoxPair(ui->Slider_rgb_G_Low,ui->spinBox_rgb_G_Low,0,255,0);
    // setupSliderSpinBoxPair(ui->Slider_rgb_B_Low,ui->spinBox_rgb_B_Low,0,255,0);

    // setupSliderSpinBoxPair(ui->Slider_rgb_R_High,ui->spinBox_rgb_R_High,0,255,0);
    // setupSliderSpinBoxPair(ui->Slider_rgb_G_High,ui->spinBox_rgb_G_High,0,255,0);
    // setupSliderSpinBoxPair(ui->Slider_rgb_B_High,ui->spinBox_rgb_B_High,0,255,0);

    // setupSliderSpinBoxPair(ui->Slider_hsv_H_Low,ui->spinBox_hsv_H_Low,0,179,0);
    // setupSliderSpinBoxPair(ui->Slider_hsv_S_Low,ui->spinBox_hsv_S_Low,0,255,0);
    // setupSliderSpinBoxPair(ui->Slider_hsv_V_Low,ui->spinBox_hsv_V_Low,0,255,0);

    // setupSliderSpinBoxPair(ui->Slider_hsv_H_High,ui->spinBox_hsv_H_High,0,179,0);
    // setupSliderSpinBoxPair(ui->Slider_hsv_S_High,ui->spinBox_hsv_S_High,0,255,0);
    // setupSliderSpinBoxPair(ui->Slider_hsv_V_High,ui->spinBox_hsv_V_High,0,255,0);

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

    connect(m_view, &ImageView::polygonDrawingFinished, this,
            [this](const QString& type, const QVector<QPointF>& points) {
                Logger::instance()->info(
                    QString("[%1] 多边形绘制完成，共 %2 个顶点")
                        .arg(type)
                        .arg(points.size())
                    );

                if (type == "region") {
                    // 区域计算现在由ExtractTabController处理
                    m_extractController->calculateRegionFeatures(points);
                }
                // 模板创建现在由 TemplateUIController 处理
            });
    // ========== PipelineManager信号 ==========
    connect(m_pipelineManager, &PipelineManager::pipelineFinished,
            [this](const QString& message) {
                ui->statusbar->showMessage(message, 2000);
            });

    connect(m_pipelineManager, &PipelineManager::algorithmQueueChanged,
            []() {
                //Logger::instance()->info(QString("算法队列数量:").arg(count));
            });

}

void MainWindow::setupSliderSpinBoxPair(QSlider *slider, QSpinBox *spinbox,
                                        int min, int max, int defaultValue)
{
    slider->setRange(min, max);
    slider->setValue(defaultValue);
    spinbox->setRange(min, max);
    spinbox->setValue(defaultValue);

    connect(slider, &QSlider::valueChanged, spinbox, &QSpinBox::setValue);
    connect(spinbox, QOverload<int>::of(&QSpinBox::valueChanged),
            slider, &QSlider::setValue);
}

// ========== 核心处理 ==========

void MainWindow::processAndDisplay()
{
    // 防止程序关闭时访问已销毁的UI对象
    // if (!ui || !ui->Slider_brightness) {
    //     qDebug() << "[MainWindow] UI已销毁，忽略processAndDisplay";
    //     return;
    // }

    // ========== 2. 颜色过滤由 FilterTabWidget 自己管理 ==========

    // ========== 3. 根据当前Tab设置显示模式 ==========
    setDisplayModeForCurrentTab();

    // ========== 4. 执行 Pipeline ==========
    cv::Mat currentImage = m_roiManager.getCurrentImage();
    const PipelineContext& result = m_pipelineManager->execute(currentImage);

    // ========== 5. 显示结果 ==========
    cv::Mat displayImage = result.getFinalDisplay();
    showImage(displayImage);

    // ========== 6. 更新判定label显示 ==========
    int count = result.currentRegions;
    ui->lineEdit_nowRegions->setText(QString::number(count));
}

void MainWindow::setDisplayModeForCurrentTab()
{
    switch (m_currentTabIndex)
    {
    case 0: m_pipelineManager->setDisplayMode(DisplayConfig::Mode::Channel); break;
    case 1: m_pipelineManager->setDisplayMode(DisplayConfig::Mode::Enhanced); break;
    case 2: m_pipelineManager->setDisplayMode(DisplayConfig::Mode::MaskGreenWhite); break;
    case 3: case 5: case 6: m_pipelineManager->setDisplayMode(DisplayConfig::Mode::MaskGreenWhite); break;
    case 4: m_pipelineManager->setDisplayMode(DisplayConfig::Mode::Processed); break;
    default: m_pipelineManager->setDisplayMode(DisplayConfig::Mode::Original); break;
    case 7: m_pipelineManager->setDisplayMode(DisplayConfig::Mode::LineDetect); break;
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

    // 第一步：打开文件对话框，让用户选择文件
    QString fileName = m_fileManager->selectImageFile(path);
    if (fileName.isEmpty()) {
        Logger::instance()->warning("用户取消了文件选择");
        return;
    }

    // 第二步：读取图像文件（通过信号返回结果）
    // 连接一次性信号，在图像加载后触发处理
    // 不太好吧
    QMetaObject::Connection conn = connect(m_fileManager, &FileManager::imageLoaded,
            this, [this](const cv::Mat&) {
                processAndDisplay();
            });

    m_fileManager->readImageFile(fileName);

    // 断开连接以避免重复触发
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

//判定

void MainWindow::on_btn_runTest_clicked()
{
    if(m_roiManager.getCurrentImage().empty())
    {
        QMessageBox::warning(this,"提示","请先打开图像");
        return;
    }
    bool ok1,ok2;
    int minRegions=ui->lineEdit_minRegionCount->text().toInt(&ok1);
    int maxRegions=ui->lineEdit_maxRegionCount->text().toInt(&ok2);

    if(!ok1 || !ok2)
    {
        QMessageBox::warning(this, "输入错误", "请输入有效的数字！");
        return;
    }
    if (minRegions < 0 || maxRegions < minRegions)
    {
        QMessageBox::warning(this, "输入错误",
                             "最小值不能为负数，且最大值不能小于最小值！");
        return;
    }
    int currentCount=m_pipelineManager->getLastContext().currentRegions;
    bool pass=(currentCount >= minRegions && currentCount <= maxRegions);

    if(pass)
    {
        QMessageBox::information(this,"判定结果",QString("✅ OK\n当前区域数: %1\n符合范围: [%2, %3]")
                                               .arg(currentCount).arg(minRegions).arg(maxRegions));
    }
    else
    {
        QMessageBox::warning(this, "判定结果",
                             QString("❌ NG\n当前区域数: %1\n要求范围: [%2, %3]")
                                 .arg(currentCount)
                                 .arg(minRegions)
                                 .arg(maxRegions));
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
    // if (m_isDestroying) {
    //     qDebug() << "[MainWindow] 程序正在关闭，忽略tabWidget_currentChanged";
    //     return;
    // }

    if(m_roiManager.getFullImage().empty()) return;

    m_currentTabIndex = index;
    processAndDisplay();
}

void MainWindow::on_btn_Log_clicked()
{
    qDebug() << "Log button clicked, switching to index 1";
    qDebug() << "Current widget count:" << ui->stackedWidget_MainWindow->count();
    ui->tabWidget->hide();
    ui->stackedWidget_MainWindow->setCurrentIndex(1);
}

void MainWindow::on_btn_Home_clicked()
{
    qDebug() << "Home button clicked, switching to index 0";
    ui->tabWidget->show();
    ui->stackedWidget_MainWindow->setCurrentIndex(0);
}
