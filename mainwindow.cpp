#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QFile>
#include <QTimer>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , m_pipelineManager(new PipelineManager(this))
{
    ui->setupUi(this);

    setupUI();
    setupConnections();
}

MainWindow::~MainWindow()
{
    delete ui;
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

    // 设置滑块-数字框对
    setupSliderSpinBoxPair(ui->Slider_brightness, ui->spinBox_brightness, -100, 100, 0);
    setupSliderSpinBoxPair(ui->Slider_contrast, ui->spinBox_contrast, 0, 300, 100);
    setupSliderSpinBoxPair(ui->Slider_gamma, ui->spinBox_gamma, 10, 300, 100);
    setupSliderSpinBoxPair(ui->Slider_sharpen, ui->spinBox_sharpen, 0, 500, 100);
    setupSliderSpinBoxPair(ui->Slider_grayLow, ui->spinBox_grayLow, 0, 255, 0);
    setupSliderSpinBoxPair(ui->Slider_grayHigh, ui->spinBox_grayHigh, 0, 255, 0);

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

    // ========== PipelineManager信号 ==========
    connect(m_pipelineManager, &PipelineManager::pipelineFinished,
            [this](const QString& message) {
                ui->statusbar->showMessage(message, 2000);
            });

    connect(m_pipelineManager, &PipelineManager::algorithmQueueChanged,
            [this](int count) {
                qDebug() << "算法队列数量:" << count;
            });

    connect(ui->comboBox_selectAlgorithm,QOverload<int>::of(&QComboBox::currentIndexChanged),
            this,&MainWindow::onAlgorithmTypeChanged);
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
    if (currentImg.empty())
    {
        //qDebug() << "[MainWindow] 当前图像为空";
        return;
    }

    // ✅ 1. 同步UI参数到Pipeline
    m_pipelineManager->syncFromUI(
        ui->Slider_brightness, ui->Slider_contrast, ui->Slider_gamma,
        ui->Slider_sharpen, ui->Slider_grayLow, ui->Slider_grayHigh
        );

    // ✅ 2. 执行Pipeline
    PipelineContext result = m_pipelineManager->execute(currentImg);

    // ✅ 3. 显示结果
    cv::Mat displayImg = result.getFinalDisplay();
    showImage(displayImg);
    int count=result.currentRegions;
    ui->lineEdit_nowRegions->setText(QString::number(count));
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
        QMessageBox::warning(this, "打开文件", "文件为空");
        return;
    }

    cv::Mat img = cv::imread(fileName.toStdString());
    if (img.empty())
    {
        QMessageBox::warning(this, "加载失败", "无法读取图像");
        return;
    }

    // ✅ 设置完整图像
    m_roiManager.setFullImage(img);
    m_view->clearRoi();

    // ✅ 显示
    //processAndDisplay();
    showImage(img);

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

}

void MainWindow::on_btn_addOption_clicked()
{
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

    processAndDisplay();
}

void MainWindow::on_btn_removeOption_clicked()
{
    int row = ui->algorithmListWidget->currentRow();
    if (row < 0) return;

    // ✅ 从Pipeline管理器移除
    m_pipelineManager->removeAlgorithmStep(row);

    // 从UI列表移除
    delete ui->algorithmListWidget->takeItem(row);

    ui->statusbar->showMessage("已移除算法");
    processAndDisplay();
}

void MainWindow::on_btn_optionUp_clicked()
{
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
    processAndDisplay();
}

// ========== 其他 ==========

void MainWindow::on_comboBox_channels_currentIndexChanged(int index)
{
    // 这个功能暂时未实现
    Q_UNUSED(index);
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

