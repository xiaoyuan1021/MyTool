#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "logger.h"
#include <QFile>
#include <QTimer>
#include <QInputDialog>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , m_pipelineManager(new PipelineManager(this))
    , m_templateManager(new TemplateMatchManager(this))
    , m_systemMonitor(new SystemMonitor(this))
    , m_isDrawingRegion(false)
    , m_polygonItem(nullptr)
{
    ui->setupUi(this);

    setupUI();
    setupConnections();

    setupSystemMonitor();

    Logger::instance()->setTextEdit(ui->textEdit_log);
    Logger::instance()->setLogFile("test.log");
    Logger::instance()->enableFileLog(true);
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::setupSystemMonitor()
{
    // 1️⃣ 绑定显示控件（假设你的 label 名称是 label_cpu 和 label_memory）
    m_systemMonitor->setLabels(ui->label_cpu, ui->label_memory);

    // 2️⃣ 设置更新间隔（1秒更新一次）
    m_systemMonitor->setUpdateInterval(1000);

    // 3️⃣ 启动监控
    m_systemMonitor->startMonitoring();

    // 4️⃣ （可选）连接信号槽，用于日志记录或其他处理
    connect(m_systemMonitor, &SystemMonitor::cpuUsageUpdated,
            this, [this](double usage) {
                // 当 CPU 占用率超过 80% 时记录日志
                if (usage > 80.0) {
                    Logger::instance()->warning(
                        QString("CPU 占用率过高: %1%").arg(usage, 0, 'f', 1)
                        );
                }
            });

    connect(m_systemMonitor, &SystemMonitor::memoryUsageUpdated,
            this, [this](double usedMB, double totalMB, double percent) {
                // 当内存使用率超过 90% 时记录日志
                if (percent > 90.0) {
                    Logger::instance()->warning(
                        QString("内存使用率过高: %1% (%2/%3 MB)")
                            .arg(percent, 0, 'f', 1)
                            .arg(usedMB, 0, 'f', 0)
                            .arg(totalMB, 0, 'f', 0)
                        );
                }
            });
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
    setupSliderSpinBoxPair(ui->Slider_brightness, ui->spinBox_brightness, -100, 100, 0);
    setupSliderSpinBoxPair(ui->Slider_contrast, ui->spinBox_contrast, 0, 300, 100);
    setupSliderSpinBoxPair(ui->Slider_gamma, ui->spinBox_gamma, 10, 300, 100);
    setupSliderSpinBoxPair(ui->Slider_sharpen, ui->spinBox_sharpen, 0, 500, 100);

    setupSliderSpinBoxPair(ui->Slider_grayLow, ui->spinBox_grayLow, 0, 255, 0);
    setupSliderSpinBoxPair(ui->Slider_grayHigh, ui->spinBox_grayHigh, 0, 255, 0);

    setupSliderSpinBoxPair(ui->Slider_rgb_R_Low,ui->spinBox_rgb_R_Low,0,255,0);
    setupSliderSpinBoxPair(ui->Slider_rgb_G_Low,ui->spinBox_rgb_G_Low,0,255,0);
    setupSliderSpinBoxPair(ui->Slider_rgb_B_Low,ui->spinBox_rgb_B_Low,0,255,0);

    setupSliderSpinBoxPair(ui->Slider_rgb_R_High,ui->spinBox_rgb_R_High,0,255,0);
    setupSliderSpinBoxPair(ui->Slider_rgb_G_High,ui->spinBox_rgb_G_High,0,255,0);
    setupSliderSpinBoxPair(ui->Slider_rgb_B_High,ui->spinBox_rgb_B_High,0,255,0);

    setupSliderSpinBoxPair(ui->Slider_hsv_H_Low,ui->spinBox_hsv_H_Low,0,179,0);
    setupSliderSpinBoxPair(ui->Slider_hsv_S_Low,ui->spinBox_hsv_S_Low,0,255,0);
    setupSliderSpinBoxPair(ui->Slider_hsv_V_Low,ui->spinBox_hsv_V_Low,0,255,0);

    setupSliderSpinBoxPair(ui->Slider_hsv_H_High,ui->spinBox_hsv_H_High,0,179,0);
    setupSliderSpinBoxPair(ui->Slider_hsv_S_High,ui->spinBox_hsv_S_High,0,255,0);
    setupSliderSpinBoxPair(ui->Slider_hsv_V_High,ui->spinBox_hsv_V_High,0,255,0);

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
                    // 区域计算
                    calculateRegionFeatures(points);
                }
                else if (type == "template") {
                    // 模板创建
                    createTemplateFromPolygon(points);
                }
            });
    // ========== PipelineManager信号 ==========
    connect(m_pipelineManager, &PipelineManager::pipelineFinished,
            [this](const QString& message) {
                ui->statusbar->showMessage(message, 2000);
            });

    connect(m_pipelineManager, &PipelineManager::algorithmQueueChanged,
            [](int count) {
                Logger::instance()->info(QString("算法队列数量:").arg(count));
            });

    connect(ui->comboBox_selectAlgorithm,QOverload<int>::of(&QComboBox::currentIndexChanged),
            this,&MainWindow::onAlgorithmTypeChanged);

    connect(ui->algorithmListWidget,&QListWidget::currentItemChanged,
            this,&MainWindow::onAlgorithmSelectionChanged);
    // ✅ 模板管理器信号
    connect(m_templateManager, &TemplateMatchManager::templateCreated,
            this, [this](const QString& name) {
                Logger::instance()->info(QString("模板已创建: %1").arg(name));
                updateTemplateList();
            });

    connect(m_templateManager, &TemplateMatchManager::matchCompleted,
            this, [](int count) {
                Logger::instance()->info(QString("匹配完成，找到 %1 个目标").arg(count));
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
    cv::Mat currentImg = m_roiManager.getCurrentImage();
    if (currentImg.empty()) return;


    // ✅ 1. 同步UI参数到Pipeline
    m_pipelineManager->syncFromUI(
        ui->Slider_brightness, ui->Slider_contrast, ui->Slider_gamma,
        ui->Slider_sharpen, ui->Slider_grayLow, ui->Slider_grayHigh
        );

    int filterMode=ui->comboBox_filterMode->currentIndex();
    cv::Mat filtered;
    switch (filterMode)
{
    case 0:
        m_pipelineManager->setGrayFilterEnabled(true);
        break;
    case 1:
    {
        m_pipelineManager->setGrayFilterEnabled(false);

        // 📖 获取UI参数（假设你有Low/High滑块）
        int rLow = ui->Slider_rgb_R_Low->value();
        int rHigh = ui->Slider_rgb_R_High->value();
        int gLow = ui->Slider_rgb_G_Low->value();
        int gHigh = ui->Slider_rgb_G_High->value();
        int bLow = ui->Slider_rgb_B_Low->value();
        int bHigh = ui->Slider_rgb_B_High->value();

        // 📖 调用我们刚写的函数
        imageprocessor processor;
        filtered = processor.filterRGB(currentImg,
                                       rLow, rHigh,
                                       gLow, gHigh,
                                       bLow, bHigh);

        // 📖 临时显示（绕过Pipeline）
        if (!filtered.empty()) {
            // 可以选择用绿白显示（更直观）
            cv::Mat display = maskToGreenWhite(filtered);
            showImage(display);
            return;
        }
        break;
    }
    case 2:
    {
        m_pipelineManager->setGrayFilterEnabled(false);

        int hLow = ui->Slider_hsv_H_Low->value();
        int hHigh = ui->Slider_hsv_H_High->value();
        int sLow = ui->Slider_hsv_S_Low->value();
        int sHigh = ui->Slider_hsv_S_High->value();
        int vLow = ui->Slider_hsv_V_Low->value();
        int vHigh = ui->Slider_hsv_V_High->value();

        imageprocessor processor;
        filtered = processor.filterHSV(currentImg,
                                       hLow, hHigh,
                                       sLow, sHigh,
                                       vLow, vHigh);

        if (!filtered.empty()) {
            cv::Mat display = maskToGreenWhite(filtered);
            showImage(display);
            return;
        }
        break;
    }
}
    if(filterMode==0)
{
    // ✅ 2. 执行Pipeline
    PipelineContext result = m_pipelineManager->execute(currentImg);

    // ✅ 3. 显示结果
    cv::Mat displayImg = result.getFinalDisplay();
    showImage(displayImg);

    int count=result.currentRegions;
    ui->lineEdit_nowRegions->setText(QString::number(count));
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
    QString fileName = QFileDialog::getOpenFileName(
        this, "选择图片", "F:/Visual Studio/opencv/lena.jpg",
        "Image(*.jpg *.png *.tif)"
        );

    if (fileName.isEmpty())
    {
        Logger::instance()->warning("用户取消了文件选择");  // ← 警告
        QMessageBox::warning(this, "打开文件", "文件为空");
        return;
    }

    cv::Mat img = cv::imread(fileName.toStdString());
    if (img.empty())
    {
        Logger::instance()->error("无法读取图像文件");  // ← 警告
        QMessageBox::warning(this, "加载失败", "无法读取图像");
        return;
    }

    // ✅ 设置完整图像
    m_roiManager.setFullImage(img);
    m_view->clearRoi();

    // ✅ 显示
    //processAndDisplay();
    showImage(img);

    Logger::instance()->info("图像加载成功! ");

    ui->statusbar->showMessage("图片加载成功", 2000);
}

void MainWindow::on_btn_saveImg_clicked()
{
    const PipelineContext& ctx = m_pipelineManager->getLastContext();

    if (ctx.srcBgr.empty())
    {
        QMessageBox::warning(this, "保存失败", "请先打开图片");
        return;
    }

    QString saveName = QFileDialog::getSaveFileName(
        this, "保存图片", ".", "*.jpg *.png *.tif"
        );

    if (saveName.isEmpty()) return;

    cv::Mat toSave;

    // 如果有mask，询问用户保存什么
    if (!ctx.mask.empty() && cv::countNonZero(ctx.mask) > 0)
    {
        QMessageBox::StandardButton reply = QMessageBox::question(
            this, "保存选项",
            "保存原图还是处理后的mask?\n"
            "Yes = 保存mask (黑白图)\n"
            "No = 保存增强后的图像",
            QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel
            );

        if (reply == QMessageBox::Yes) {
            toSave = ctx.mask;
        } else if (reply == QMessageBox::No) {
            toSave = ctx.enhanced.empty() ? ctx.srcBgr : ctx.enhanced;
        } else {
            return;
        }
    }
    else
    {
        toSave = ctx.enhanced.empty() ? ctx.srcBgr : ctx.enhanced;
    }

    cv::imwrite(saveName.toStdString(), toSave);
    ui->statusbar->showMessage("图片保存成功", 2000);
}

// ========== 参数调整 ==========

void MainWindow::on_Slider_brightness_valueChanged(int)
{
    m_pipelineManager->setGrayFilterEnabled(false);
    processAndDisplay();
}

void MainWindow::on_Slider_contrast_valueChanged(int)
{
    m_pipelineManager->setGrayFilterEnabled(false);
    processAndDisplay();
}

void MainWindow::on_Slider_gamma_valueChanged(int)
{
    m_pipelineManager->setGrayFilterEnabled(false);
    processAndDisplay();
}

void MainWindow::on_Slider_sharpen_valueChanged(int)
{
    m_pipelineManager->setGrayFilterEnabled(false);
    processAndDisplay();
}

void MainWindow::on_Slider_grayLow_valueChanged(int)
{
    m_pipelineManager->setGrayFilterEnabled(true);
    processAndDisplay();
}

void MainWindow::on_Slider_grayHigh_valueChanged(int)
{
    m_pipelineManager->setGrayFilterEnabled(true);
    processAndDisplay();
}

void MainWindow::on_btn_resetBC_clicked()
{
    ui->Slider_brightness->setValue(0);
    ui->Slider_contrast->setValue(100);
    ui->Slider_gamma->setValue(100);
    ui->Slider_sharpen->setValue(100);

    m_pipelineManager->resetEnhancement();
    m_pipelineManager->setGrayFilterEnabled(false);

    processAndDisplay();
}

// ========== ROI操作 ==========

void MainWindow::on_btn_drawRoi_clicked()
{
    if (m_roiManager.getFullImage().empty()) return;

    m_view->setRoiMode(true);
    ui->statusbar->showMessage("请按下左键绘制ROI");
}

void MainWindow::on_btn_resetROI_clicked()
{
    if (m_roiManager.getFullImage().empty()) return;

    m_roiManager.resetRoi();
    m_view->clearRoi();

    showImage(m_roiManager.getFullImage());
    ui->statusbar->showMessage("ROI已重置，处理使用完整图像", 2000);
    Logger::instance()->info("ROI已重置");
}

void MainWindow::onRoiSelected(const QRectF &roiImgRectF)
{
    if (!m_roiManager.applyRoi(roiImgRectF))
    {
        ui->statusbar->showMessage("ROI应用失败", 2000);
        return;
    }

    processAndDisplay();

    cv::Rect roi = m_roiManager.getLastRoi();
    ui->statusbar->showMessage(
        QString("ROI已选择：x=%1 y=%2 w=%3 h=%4")
            .arg(roi.x).arg(roi.y).arg(roi.width).arg(roi.height),
        2000
        );
}

// ========== 算法队列操作 ==========

void MainWindow::on_comboBox_selectAlgorithm_currentIndexChanged(int index)
{
    Q_UNUSED(index);
}

void MainWindow::on_btn_addOption_clicked()
{
    saveCurrentEdit();

    AlgorithmStep step;
    int index = ui->comboBox_selectAlgorithm->currentIndex();
    step.name = ui->comboBox_selectAlgorithm->currentText();
    step.type = "HalconAlgorithm";
    step.enabled = true;
    step.description = "Halcon图像处理算法";
    step.params["HalconAlgoType"] = index;

    switch(index)
    {
    case 0: case 2: case 4: case 6:
        step.params["radius"] = ui->doubleSpinBox_radius->value();
        break;
    case 1: case 3: case 5: case 7:
        step.params["width"] =  ui->spinBox_width->value();
        step.params["height"] = ui->spinBox_height->value();
        break;
    case 11:
        step.params["shapeType"] = ui->comboBox_shapeType->currentData().toString();
        break;
    }
    m_pipelineManager->addAlgorithmStep(step);
    QListWidgetItem *item = new QListWidgetItem(step.name);
    ui->algorithmListWidget->addItem(item);

    Logger::instance()->info(QString("添加算法 %1").arg(step.name));

    processAndDisplay();
}

void MainWindow::on_btn_removeOption_clicked()
{
    saveCurrentEdit();
    int row = ui->algorithmListWidget->currentRow();
    if (row < 0) return;

    // ✅ 从Pipeline管理器移除
    m_pipelineManager->removeAlgorithmStep(row);

    // 从UI列表移除
    delete ui->algorithmListWidget->takeItem(row);

    //ui->statusbar->showMessage("已移除算法");
    Logger::instance()->info("移除算法");
    processAndDisplay();
}

void MainWindow::on_btn_optionUp_clicked()
{
    saveCurrentEdit();
    int currentRow = ui->algorithmListWidget->currentRow();

    // 边界检查
    if (currentRow <= 0) {
        return;
    }

    // 1️⃣ 交换 Pipeline 中的算法队列数据
    m_pipelineManager->swapAlgorithmStep(currentRow, currentRow - 1);

    // 2️⃣ 交换 UI 列表项
    QListWidgetItem *item = ui->algorithmListWidget->takeItem(currentRow);  // 取出当前项
    ui->algorithmListWidget->insertItem(currentRow - 1, item);              // 插入到上一行

    // 3️⃣ 保持选中状态
    ui->algorithmListWidget->setCurrentRow(currentRow - 1);

    // 4️⃣ 重新处理图像（因为算法顺序变了）
    processAndDisplay();

    ui->statusbar->showMessage("算法步骤已上移", 1000);
}

void MainWindow::on_btn_optionDown_clicked()
{
    saveCurrentEdit();
    int currentRow = ui->algorithmListWidget->currentRow();

    // 边界检查
    if (currentRow < 0 || currentRow >= ui->algorithmListWidget->count() - 1) {
        return;
    }

    // 1️⃣ 交换 Pipeline 中的算法队列数据
    m_pipelineManager->swapAlgorithmStep(currentRow, currentRow + 1);

    // 2️⃣ 交换 UI 列表项
    QListWidgetItem *item = ui->algorithmListWidget->takeItem(currentRow);  // 取出当前项
    ui->algorithmListWidget->insertItem(currentRow + 1, item);              // 插入到下一行

    // 3️⃣ 保持选中状态
    ui->algorithmListWidget->setCurrentRow(currentRow + 1);

    // 4️⃣ 重新处理图像
    processAndDisplay();

    ui->statusbar->showMessage("算法步骤已下移", 1000);
}

void MainWindow::onAlgorithmTypeChanged(int index)
{
    switch (index)
    {
    case 0: case 2: case 4: case 6:
        ui->stackedWidget_Algorithm->setCurrentIndex(0);
        break;
    case 1: case 3: case 5: case 7:
        ui->stackedWidget_Algorithm->setCurrentIndex(1);
        break;
    case 8: case 9: case 10:
        ui->stackedWidget_Algorithm->setCurrentIndex(2);
        break;
    case 11:
        ui->stackedWidget_Algorithm->setCurrentIndex(3);
        break;
    }
}

// ========== 区域筛选 ==========

void MainWindow::on_comboBox_select_currentIndexChanged(int index)
{
    switch(index)
    {
    case 0:  // 面积
        ui->lineEdit_minArea->setPlaceholderText("例如: 50");
        ui->lineEdit_maxArea->setPlaceholderText("例如: 1000");
        break;
    case 1:  // 圆度
        ui->lineEdit_minArea->setPlaceholderText("例如: 0.8");
        ui->lineEdit_maxArea->setPlaceholderText("例如: 1.0");
        break;
    case 2:  // 宽度
        ui->lineEdit_minArea->setPlaceholderText("例如: 10");
        ui->lineEdit_maxArea->setPlaceholderText("例如: 100");
        break;
    case 3:  // 高度
        ui->lineEdit_minArea->setPlaceholderText("例如: 10");
        ui->lineEdit_maxArea->setPlaceholderText("例如: 100");
        break;
    }
}

void MainWindow::on_comboBox_condition_currentIndexChanged(int index)
{
    // 0 = 满足所有条件 (AND)
    // 1 = 满足任意条件 (OR)
    FilterMode mode = (index == 0) ? FilterMode::And : FilterMode::Or;
    m_pipelineManager->setFilterMode(mode);

    qDebug() << "筛选模式已切换:" << getFilterModeName(mode);
}


void MainWindow::on_btn_clearFilter_clicked()
{
    // 清除所有筛选条件
    m_pipelineManager->clearShapeFilter();

    // 清空输入框
    ui->lineEdit_minArea->clear();
    ui->lineEdit_maxArea->clear();

    // 重新执行Pipeline
    processAndDisplay();

    ui->statusbar->showMessage("已清除所有筛选条件", 2000);
}


void MainWindow::on_btn_addFilter_clicked()
{
    const PipelineContext& ctx = m_pipelineManager->getLastContext();

    if (ctx.processed.empty())
    {
        QMessageBox::warning(this, "提示", "请先执行算法处理!");
        return;
    }

    // 1️⃣ 获取用户输入
    bool ok1, ok2;
    double minValue = ui->lineEdit_minArea->text().toDouble(&ok1);
    double maxValue = ui->lineEdit_maxArea->text().toDouble(&ok2);

    if (!ok1 || !ok2 || minValue < 0 || maxValue < minValue)
    {
        QMessageBox::warning(this, "输入错误", "请输入有效的范围!");
        return;
    }

    // 2️⃣ 获取选择的特征类型
    int featureIndex = ui->comboBox_select->currentIndex();
    ShapeFeature feature;

    switch(featureIndex)
    {
    case 0:  feature = ShapeFeature::Area; break;
    case 1:  feature = ShapeFeature::Circularity; break;
    case 2:  feature = ShapeFeature::Width; break;
    case 3:  feature = ShapeFeature::Height; break;
    case 4:  feature = ShapeFeature::Compactness; break;
    case 5:  feature = ShapeFeature::Convexity; break;
    default: feature = ShapeFeature::Area;
    }

    // 3️⃣ 添加筛选条件
    FilterCondition condition(feature, minValue, maxValue);

    // ✅ 方式1：清除旧条件，只保留当前条件（单特征筛选）
    // m_pipelineManager->clearShapeFilter();
    // m_pipelineManager->addFilterCondition(condition);

    // ✅ 方式2：累加条件（多特征组合筛选）
    // 如果你想支持用户多次点击"筛选"来添加多个条件：
    m_pipelineManager->addFilterCondition(condition);

    ui->statusbar->showMessage(
        QString("已应用筛选: %1").arg(condition.toString()),
        2000
        );

}

//提取按钮
void MainWindow::on_btn_select_clicked()
{
    //重新执行Pipeline
    m_view->clearPolygon();
    m_drawnpoints.clear();
    processAndDisplay();
}

// ========== 颜色通道 ==========

void MainWindow::on_comboBox_channels_currentIndexChanged(int index)
{
    if(m_roiManager.getCurrentImage().empty()) return;
    switch (index)
    {
    case 0:
        m_pipelineManager->setChannelMode(PipelineConfig::Channel::Gray);
        Logger::instance()->info("切换到灰度模式");
        break;
    case 1:
        m_pipelineManager->setChannelMode(PipelineConfig::Channel::RGB);
        Logger::instance()->info("切换到RGB模式");
        break;
    case 2:
        m_pipelineManager->setChannelMode(PipelineConfig::Channel::HSV);
        Logger::instance()->info("切换到HSV模式");
        break;
    case 3:
        m_pipelineManager->setChannelMode(PipelineConfig::Channel::B);
        Logger::instance()->info("切换到B通道");
        break;
    case 4:
        m_pipelineManager->setChannelMode(PipelineConfig::Channel::G);
        Logger::instance()->info("切换到G通道");
        break;
    case 5:
        m_pipelineManager->setChannelMode(PipelineConfig::Channel::R);
        Logger::instance()->info("切换到R通道");
        break;
    default:
        Logger::instance()->warning("未知的通道类型");
        return;
    }
    processAndDisplay();
    ui->statusbar->showMessage(QString("已切换到 %1")
                                   .arg(ui->comboBox_channels->currentText()));
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

void MainWindow::on_btn_clearLog_clicked()
{
    Logger::instance()->clear();
    Logger::instance()->info("日志已清空");
}


void MainWindow::on_btn_drawRegion_clicked()
{
    if(m_roiManager.getCurrentImage().empty())
    {
        Logger::instance()->warning("请先打开图像");
        return;
    }

    m_view->startPolygonDrawing("region");


    ui->statusbar->showMessage("请在图像上点击左键添加顶点，右键完成绘制");
}

void MainWindow::on_btn_clearRegion_clicked()
{
    m_view->clearPolygonDrawing();
    ui->statusbar->showMessage("已清除绘制区域");
}

void MainWindow::calculateRegionFeatures(const QVector<QPointF>& points)
{
    if (points.size() < 3) {
        Logger::instance()->warning("顶点数量不足，至少需要3个点");
        return;
    }

    const PipelineContext& ctx = m_pipelineManager->getLastContext();

    if (ctx.processed.empty() && ctx.mask.empty()) {
        Logger::instance()->warning("请先执行算法处理，然后再绘制区域");
        return;
    }

    // ========== 1. 获取处理后的图像 ==========
    cv::Mat processedImg = ctx.processed.empty() ? ctx.mask : ctx.processed;

    // ========== 2. 调用 HalconAlgorithm 进行区域分析 ==========
    HalconAlgorithm analyzer;
    QVector<RegionFeature> features = analyzer.analyzeRegionsInPolygon(points, processedImg);

    // ========== 3. 如果没找到目标，直接返回 ==========
    if (features.isEmpty()) {
        return;  // analyzeRegionsInPolygon 内部已经输出了警告日志
    }

    // ========== 4. 输出结果到日志 ==========
    Logger::instance()->info("========== ROI 区域特征分析 ==========");
    Logger::instance()->info(QString("找到 %1 个连通域").arg(features.size()));
    Logger::instance()->info("-----------------------------------");

    for (const auto& feature : features) {
        Logger::instance()->info(feature.toString());
    }
    Logger::instance()->info("======================================");
}



//补正 模板匹配

void MainWindow::on_btn_drawTemplate_clicked()
{
    if(m_roiManager.getCurrentImage().empty())
    {
        Logger::instance()->info("请先打开图像");
        return;
    }
    m_view->startPolygonDrawing("template");
    ui->statusbar->showMessage("请在图像上绘制模板区域");

}

void MainWindow::on_btn_clearTemplate_clicked()
{
    m_view->clearPolygonDrawing();
    Logger::instance()->info("已清除模板区域");
}

void MainWindow::createTemplateFromPolygon(const QVector<QPointF> &points)
{
    if (points.size() < 3) {
        Logger::instance()->warning("模板顶点数量不足");
        return;
    }

    Logger::instance()->info("========== 创建模板 ==========");
    Logger::instance()->info(QString("模板顶点数: %1").arg(points.size()));
}

void MainWindow::on_btn_creatTemplate_clicked()
{
    QVector<QPointF> points = m_view->getPolygonPoints();

    if (points.size() < 3) {
        QMessageBox::warning(this, "提示", "请先绘制模板区域！");
        return;
    }

    // 弹出对话框输入模板名称
    bool ok;
    QString name = QInputDialog::getText(
        this, "创建模板", "请输入模板名称:",
        QLineEdit::Normal, "Template_1", &ok
        );

    if (!ok || name.isEmpty()) {
        return;
    }

    // 使用默认参数创建模板
    TemplateData params = m_templateManager->getDefaultParams();

    bool success = m_templateManager->createTemplate(
        name,
        m_roiManager.getCurrentImage(),
        points,
        params
        );

    if (success) {
        QMessageBox::information(this, "成功", "模板创建成功！");
        m_view->clearPolygonDrawing();
    } else {
        QMessageBox::warning(this, "失败", "模板创建失败，请查看日志");
    }
}

void MainWindow::on_btn_findTemplate_clicked()
{
    if (m_templateManager->getTemplateCount() == 0) {
        QMessageBox::warning(this, "提示", "请先创建模板！");
        return;
    }

    if (m_roiManager.getCurrentImage().empty()) {
        QMessageBox::warning(this, "提示", "请先打开图像！");
        return;
    }

    // 使用第一个模板进行匹配
    QVector<MatchResult> results = m_templateManager->findTemplate(
        m_roiManager.getCurrentImage(),
        0,      // templateIndex
        0.6,    // minScore
        3,     // maxMatches
        0.6     // greediness
        );

    // 显示结果

    findAndDisplayMatches(results);

    Logger::instance()->info("========== 匹配结果 ==========");
    for (int i = 0; i < results.size(); ++i) {
        Logger::instance()->info(
            QString("[%1] %2").arg(i + 1).arg(results[i].toString())
            );
    }
    Logger::instance()->info("==============================");

    if (results.isEmpty()) {
        QMessageBox::information(this, "结果", "未找到匹配目标");
    } else {
        QMessageBox::information(this, "结果",
                                 QString("找到 %1 个匹配目标").arg(results.size()));
    }
}

void MainWindow::onAlgorithmSelectionChanged(QListWidgetItem *current, QListWidgetItem *previous)
{
    Q_UNUSED(previous);  // 不需要用到

    // 1️⃣ 如果之前在编辑，先保存
    if (m_editingAlgorithmIndex >= 0)
    {
        saveCurrentEdit();
    }

    // 2️⃣ 加载新选中项的参数
    if (current)
    {
        int newIndex = ui->algorithmListWidget->row(current);
        loadAlgorithmParameters(newIndex);
        m_editingAlgorithmIndex = newIndex;

        ui->statusbar->showMessage(
            QString("正在编辑: %1 (修改参数后点击其他项自动保存)")
                .arg(current->text()),
            3000
            );
    }
    else
    {
        // 没有选中项，重置编辑状态
        m_editingAlgorithmIndex = -1;
    }
}

void MainWindow::saveCurrentEdit()
{
    // 如果没有在编辑，直接返回
    if (m_editingAlgorithmIndex < 0)
    {
        return;
    }

    // 1️⃣ 获取当前编辑的算法
    const QVector<AlgorithmStep>& queue = m_pipelineManager->getAlgorithmQueue();
    if (m_editingAlgorithmIndex >= queue.size())
    {
        m_editingAlgorithmIndex = -1;
        return;
    }

    AlgorithmStep step = queue[m_editingAlgorithmIndex];  // 复制一份

    // 2️⃣ 从UI读取修改后的参数
    int algoType = step.params["HalconAlgoType"].toInt();

    switch(algoType)
    {
    case 0: case 2: case 4: case 6:  // 圆形算法
        step.params["radius"] = ui->doubleSpinBox_radius->value();
        break;

    case 1: case 3: case 5: case 7:  // 矩形算法
        step.params["width"] = ui->spinBox_width->value();
        step.params["height"] = ui->spinBox_height->value();
        break;

    case 11:  // 形状变换
        step.params["shapeType"] = ui->comboBox_shapeType->currentData().toString();
        break;
    }

    // 3️⃣ 更新到PipelineManager
    m_pipelineManager->updateAlgorithmStep(m_editingAlgorithmIndex, step);

    // 4️⃣ 重新处理图像
    processAndDisplay();

    // 5️⃣ 提示用户
    Logger::instance()->info(
        QString("已保存算法 #%1: %2 的参数修改")
            .arg(m_editingAlgorithmIndex + 1).arg(step.name)
        );
}

void MainWindow::loadAlgorithmParameters(int index)
{
    // 1️⃣ 获取算法步骤
    const QVector<AlgorithmStep>& queue = m_pipelineManager->getAlgorithmQueue();
    if (index < 0 || index >= queue.size()) {
        return;
    }

    const AlgorithmStep& step = queue[index];
    int algoType = step.params["HalconAlgoType"].toInt();

    // 2️⃣ 切换到对应页面并填充参数
    switch(algoType)
    {
    case 0: case 2: case 4: case 6:  // 圆形算法
        ui->stackedWidget_Algorithm->setCurrentIndex(0);
        ui->doubleSpinBox_radius->setValue(
            step.params.value("radius", 3.5).toDouble()
            );
        break;

    case 1: case 3: case 5: case 7:  // 矩形算法
        ui->stackedWidget_Algorithm->setCurrentIndex(1);
        ui->spinBox_width->setValue(
            step.params.value("width", 5).toInt()
            );
        ui->spinBox_height->setValue(
            step.params.value("height", 5).toInt()
            );
        break;

    case 8: case 9: case 10:  // 无参数算法
        ui->stackedWidget_Algorithm->setCurrentIndex(2);
        break;

    case 11:  // 形状变换
        ui->stackedWidget_Algorithm->setCurrentIndex(3);
        QString shapeType = step.params.value("shapeType", "convex").toString();
        int comboIndex = ui->comboBox_shapeType->findData(shapeType);
        if (comboIndex >= 0) {
            ui->comboBox_shapeType->setCurrentIndex(comboIndex);
        }
        break;
    }

    // 3️⃣ 提示用户
    // Logger::instance()->info(
    //     QString("已加载算法 #%1: %2 的参数").arg(index + 1).arg(step.name)
    //     );
}

void MainWindow::findAndDisplayMatches(const QVector<MatchResult> &results)
{

    // 1️⃣ 获取当前图像
    cv::Mat currentImage = m_roiManager.getCurrentImage();
    if (currentImage.empty()) {
        Logger::instance()->warning("当前没有图像");
        return;
    }

    // 2️⃣ 获取匹配参数
    int templateIndex = 0;
    double minScore = ui->doubleSpinBox_minScore->value();
    int maxMatches = ui->spinBox_matchNumber->value();

    // 3️⃣ 执行匹配
    QVector<MatchResult> matches = m_templateManager->findTemplate(
        currentImage,
        templateIndex,
        minScore,
        maxMatches
        );

    // 4️⃣ ✅ 核心改进: 使用Manager的方法绘制结果
    cv::Mat resultImage = m_templateManager->drawMatches(
        currentImage,
        templateIndex,
        matches
        );

    // 5️⃣ 显示结果
    QImage qimg = ImageUtils::Mat2Qimage(resultImage);
    m_view->setImage(qimg);

    // 6️⃣ 更新UI状态
    QString statusMsg = matches.isEmpty()
                            ? "未找到匹配"
                            : QString("找到 %1 个匹配").arg(matches.size());

    ui->statusbar->showMessage(statusMsg, 3000);
}

void MainWindow::updateTemplateList()
{

}



void MainWindow::on_btn_openLog_clicked()
{
    Logger::instance()->openLogFolder(true);
}

void MainWindow::on_comboBox_filterMode_currentIndexChanged(int index)
{
    switch (index)
    {
    case 0:
        ui->stackedWidget_filter->setCurrentIndex(0);
        break;
    case 1:
        ui->stackedWidget_filter->setCurrentIndex(1);
        break;
    case 2:
        ui->stackedWidget_filter->setCurrentIndex(2);
        break;
    default:
        ui->stackedWidget_filter->setCurrentIndex(0);
        break;
    }
    processAndDisplay();
}


void MainWindow::on_Slider_rgb_R_Low_valueChanged(int value)
{
    processAndDisplay();
}


void MainWindow::on_Slider_rgb_R_High_valueChanged(int value)
{
    processAndDisplay();
}


void MainWindow::on_Slider_rgb_G_Low_valueChanged(int value)
{
    processAndDisplay();
}


void MainWindow::on_Slider_rgb_G_High_valueChanged(int value)
{
    processAndDisplay();
}


void MainWindow::on_Slider_rgb_B_Low_valueChanged(int value)
{
    processAndDisplay();
}


void MainWindow::on_Slider_rgb_B_High_valueChanged(int value)
{
    processAndDisplay();
}


void MainWindow::on_Slider_hsv_H_Low_valueChanged(int value)
{
    processAndDisplay();
}


void MainWindow::on_Slider_hsv_H_High_valueChanged(int value)
{
    processAndDisplay();
}


void MainWindow::on_Slider_hsv_S_Low_valueChanged(int value)
{
    processAndDisplay();
}


void MainWindow::on_Slider_hsv_S_High_valueChanged(int value)
{
    processAndDisplay();
}


void MainWindow::on_Slider_hsv_V_Low_valueChanged(int value)
{
    processAndDisplay();
}


void MainWindow::on_Slider_hsv_V_High_valueChanged(int value)
{
    processAndDisplay();
}
