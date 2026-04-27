# VisionTool 项目分析文档

> 基于 Qt 6.9.1 + OpenCV 4.x 的桌面机器视觉检测工具

---

## 一、项目架构

### 1.1 整体架构概览

项目采用 **MVC 变体架构**，分为以下层次：

```
┌──────────────────────────────────────────────────────────────────┐
│                        UI 层 (MainWindow)                         │
│  ┌────────────┐  ┌──────────────┐  ┌─────────────────────────┐  │
│  │ FileManager │  │  ImageView   │  │      RoiManager         │  │
│  │ (文件IO)    │  │ (图像显示)    │  │ (多图 + 多ROI管理)      │  │
│  └────────────┘  └──────────────┘  └─────────────────────────┘  │
│  ┌──────────────────────────────────────────────────────────┐   │
│  │                     TabManager                            │   │
│  │  Image│Enhance│Filter│Process│Extract│Judge│Line│Barcode... │   │
│  └──────────────────────────────────────────────────────────┘   │
├──────────────────────────────────────────────────────────────────┤
│                      控制器层 (Controllers)                       │
│  RoiUiController │ DetectionUiController │ ConfigController      │
│  AutoDetectionController │ SignalConnector                      │
├──────────────────────────────────────────────────────────────────┤
│                      核心引擎层 (Core)                            │
│  ┌──────────────────┐  ┌──────────────────┐  ┌──────────────┐  │
│  │  PipelineManager  │  │    Pipeline      │  │ VideoManager │  │
│  │  (配置管理+调度)   │  │ (步骤链执行器)    │  │ (视频/相机)   │  │
│  └──────────────────┘  └──────────────────┘  └──────────────┘  │
│  ┌──────────────────┐  ┌──────────────────┐                     │
│  │   MqttClient     │  │   Algorithm      │                     │
│  │  (结果发布)       │  │ (算法库)         │                     │
│  └──────────────────┘  └──────────────────┘                     │
├──────────────────────────────────────────────────────────────────┤
│                      配置层 (Config)                              │
│  ConfigManager │ PipelineConfigMapper │ RoiConfig / DetectionConfig │
└──────────────────────────────────────────────────────────────────┘
```

### 1.2 目录结构

```
F:\QT\MyTool/
├── CMakeLists.txt                 # 构建文件 (CMake + Ninja)
├── app_config.json                # 默认应用配置
├── src/                           # C++ 源文件
│   ├── main.cpp                   # 入口
│   ├── core/                      # 核心引擎
│   │   ├── pipeline.cpp           # Pipeline 执行器
│   │   ├── pipeline_manager.cpp   # 管线管理器
│   │   ├── pipeline_steps.cpp     # 所有处理步骤实现
│   │   ├── video_manager.cpp      # 视频/相机管理
│   │   └── mqtt_client.cpp        # MQTT 客户端
│   ├── algorithm/                 # 算法实现
│   │   ├── image_processor.cpp    # 图像处理 (色彩/增强/滤波)
│   │   ├── opencv_algorithm.cpp   # 形态学/特征分析
│   │   ├── dnn_inference.cpp      # DNN 推理 (YOLO/ONNX)
│   │   ├── zxing_barcode_reader.cpp # 条码识别
│   │   ├── match_strategy.cpp     # 模板匹配策略
│   │   └── image_utils.cpp        # 工具函数 (Mat/QImage互转)
│   ├── config/                    # 配置管理
│   │   ├── config_manager.cpp     # JSON 配置读写
│   │   └── pipeline_config_mapper.cpp # UI-配置双向同步
│   ├── ui/                        # UI 视图层
│   │   ├── mainwindow.cpp         # 主窗口
│   │   ├── image_view.cpp         # 自定义 QGraphicsView
│   │   ├── roi_manager.cpp        # 多图多ROI管理
│   │   ├── logger.cpp             # 日志系统
│   │   ├── pipeline_result_handler.cpp # 管线结果处理
│   │   ├── signal_connector.cpp   # 信号槽集中连接
│   │   └── ...
│   ├── widgets/                   # 标签页控件
│   │   ├── tab_manager.cpp        # 懒加载标签管理器
│   │   ├── enhance_tab_widget.cpp # 增强参数面板
│   │   ├── process_tab_widget.cpp # 算法队列面板
│   │   └── ... (共12个标签页)
│   └── controllers/               # 控制器
│       ├── roi_ui_controller.cpp
│       ├── detection_ui_controller.cpp
│       ├── config_controller.cpp
│       └── auto_detection_controller.cpp
├── include/                       # 头文件 (镜像 src/ 结构)
├── ui/                            # Qt Designer .ui 文件
│   ├── mainwindow.ui
│   └── tabs/                      # 各标签页 .ui 文件
├── resources/                     # 资源文件
│   ├── res.qrc                    # Qt 资源
│   ├── style.qss                  # 全局样式表
│   ├── icons/                     # 图标
│   └── models/                    # DNN 模型
├── 3rdparty/                      # 第三方库
│   ├── opencv/                    # OpenCV 4.x
│   ├── spdlog/                    # 日志库
│   ├── zxing/                     # ZXing-CPP (条码)
│   ├── paho-mqtt/                 # MQTT C++ 客户端
│   └── stb/                       # stb_image
└── images/                        # 测试图片
```

### 1.3 核心设计模式

| 模式 | 用途 | 位置 |
|------|------|------|
| **Pipeline (管线链)** | 顺序执行图像处理步骤链 | `pipeline.cpp` / `pipeline_steps.cpp` |
| **策略模式** | 可替换的模板匹配算法 | `match_strategy.cpp` / `IMatchStrategy` |
| **单例模式** | 日志、配置管理 | `Logger::instance()` / `ConfigManager::instance()` |
| **观察者模式** | Qt 信号槽解耦模块通信 | `signal_connector.cpp` 集中连接 |
| **控制器 (MVC)** | 从 MainWindow 抽取 UI 逻辑 | `controllers/` 目录 |
| **懒加载** | 按需创建标签页控件 | `TabManager::ensureTab()` |
| **防抖 (Debounce)** | 滑块调节时延迟触发处理 | `QTimer` + `setSingleShot(true)` |

---

## 二、关键技术点与语法

### 2.1 Qt 关键特性

#### 信号槽机制

```cpp
// 自定义信号
signals:
    void imageLoaded(const QImage& image, const QString& path);

// Lambda 连接
connect(sender, &Sender::signal, this, [this](const QImage& img) {
    m_imageView->displayImage(img);
});

// 重载信号需转型
connect(spinBox, QOverload<int>::of(&QSpinBox::valueChanged),
        this, &MainWindow::onValueChanged);
```

#### QGraphicsView 框架

项目使用 `QGraphicsView + QGraphicsScene` 实现图像显示与交互：
- `ImageView` 继承 `QGraphicsView`，支持滚轮缩放、拖动
- 使用 `QGraphicsRectItem` / `QGraphicsPathItem` / `QGraphicsLineItem` 绘制 ROI、多边形、参考线
- 拖拽手柄 (drag handles) 实现 ROI 交互式调整

#### 异步处理

```cpp
// 使用 QtConcurrent 在后台线程执行管线
QFuture<PipelineContext> future = QtConcurrent::run([=]() {
    return m_pipelineManager->execute(image, config);
});
// QFutureWatcher 监听完成
QFutureWatcher<PipelineContext>* watcher = new QFutureWatcher(this);
connect(watcher, &QFutureWatcher::finished, this, [=]() {
    PipelineContext ctx = watcher->result();
    handleResult(ctx);
    watcher->deleteLater();
});
watcher->setFuture(future);
```

#### 资源系统

```cmake
# CMake 自动处理 .qrc / .ui / MOC
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC_SEARCH_PATHS "${PROJECT_SOURCE_DIR}/ui")
```

#### 样式表 (QSS)

全局样式定义在 `resources/style.qss`，应用方式：
```cpp
QFile styleFile(":/style.qss");
styleFile.open(QFile::ReadOnly);
qApp->setStyleSheet(styleFile.readAll());
```

### 2.2 管线系统 (Pipeline)

管线是项目的核心架构模式，定义如下：

```cpp
// 步骤接口
class IPipelineStep {
public:
    virtual ~IPipelineStep() = default;
    virtual void run(PipelineContext& ctx) = 0;  // 每个步骤修改上下文
};

// 管线执行器
class Pipeline {
    std::vector<std::unique_ptr<IPipelineStep>> m_steps;
public:
    void run(PipelineContext& ctx) {
        for (auto& step : m_steps) {
            step->run(ctx);
        }
    }
};
```

处理步骤执行顺序：
1. **StepColorChannel** → 通道提取 (Gray/RGB/HSV)
2. **StepEnhance** → 增强 (亮度/对比度/Gamma/锐化)
3. **StepGrayFilter** → 灰度阈值滤波
4. **StepColorFilter** → 颜色范围滤波 (RGB/HSV)
5. **StepAlgorithmQueue** → 形态学操作链 (开/闭/腐蚀/膨胀/孔洞填充)
6. **StepShapeFilter** → 形状筛选 (面积/圆度/矩形度等)
7. **StepLineDetect** → 直线检测 (HoughP/LSD/EDlines)
8. **StepReferenceLineFilter** → 参考线匹配
9. **StepBarcodeRecognition** → 条码/二维码识别

### 2.3 检测类型系统 (6 种)

```cpp
enum class DetectionType {
    Blob,             // 斑点分析
    Line,             // 直线检测
    Barcode,          // 条码/二维码
    Template,         // 模板匹配
    VideoSource,      // 视频/相机
    ObjectDetection   // DNN 目标检测
};
```

### 2.4 线程安全模型

```
UI 线程 (MainWindow)             后台线程 (QtConcurrent::run)
    │                                      │
    │ setConfig()                           │
    │ QTimer::singleShot(150ms)             │  ← 防抖
    │ processAndDisplay():                  │
    │   config = snapshot()                 │
    │   future = QtConcurrent::run(execute) │
    │   watcher.setFuture(future)           │
    │                                      │
    │                          execute():   │
    │                            mutex.lock │
    │                            pipeline.run(ctx)
    │                            mutex.unlock│
    │                                      │
    │ ← watcher::finished ──────────────── │
    │   PipelineResultHandler 处理结果      │
```

- 线程间通过 `PipelineConfig` 值拷贝（快照）通信
- `PipelineManager` 使用 `QMutex + QAtomicInt` 处理并发配置更新
- 使用"待处理配置"（pending config）模式：处理中收到新配置时标记待处理，当前帧结束后自动启用

### 2.5 多图多ROI管理

- `RoiManager` 使用 `QMap<QString, ImageRois>` 管理多张图片及其 ROI 集合
- 每张图片可包含多个 `RoiConfig`，每个 ROI 可配置多个 `DetectionItem`
- ROIs 可独立导出/导入为 JSON 配置

### 2.6 第三方库集成

| 库 | 集成方式 | 用途 |
|----|---------|------|
| OpenCV 4.x | `find_library` + `target_link_libraries` | 图像处理、DNN、视频IO |
| spdlog | 头文件库 (header-only) | 高性能日志 |
| ZXing-CPP | 静态链接 `.lib`，源码在 `3rdparty/` | 条码/二维码解码 |
| Paho MQTT | 动态链接 `.dll`，POST_BUILD 复制 DLL | MQTT 结果发布 |
| stb_image | 单头文件库 | 额外图片格式支持 |

---

## 三、项目优缺点

### 优势

1. **架构清晰**：Pipeline 管线模式将图像处理流程抽象为可配置的步骤链，结构清晰，易于扩展新算法步骤
2. **UI/逻辑分离**：Controller 层从 MainWindow 中提取了大量 UI 逻辑，SignalConnector 集中管理信号槽连接，降低了耦合度
3. **异步非阻塞**：管线处理在后台线程运行，UI 保持响应，配合防抖机制避免高频触发
4. **功能全面**：覆盖了机器视觉检测的完整流程——从图像输入、预处理、增强、滤波、形态学、特征提取、直线检测、条码识别、模板匹配到 DNN 目标检测，并支持 MQTT 结果输出
5. **多图多ROI**：支持同时加载多张图片，每张图片管理多个 ROI 区域，适合批量检测场景
6. **配置持久化**：完整的 JSON 配置序列化，支持保存/加载完整应用状态
7. **线程安全设计成熟**：QMutex + QAtomicInt + pending config 模式较好地处理了 UI 配置更新与后台处理的并发问题

### 不足

1. **CMake 使用 file(GLOB)**：`CMakeLists.txt` 中使用 `file(GLOB ...)` 收集源文件，这不会自动检测新增/删除文件，更改文件结构后需手动重新运行 cmake 配置
2. **头文件分散**：`include/` 目录镜像 `src/` 目录结构是 Visual Studio 的常见做法，但在 CMake 项目中直接 `src/` + 内联头文件更简洁
3. **MainWindow 仍偏重**：尽管分离了 Controllers，MainWindow 仍承担了大量协调职责（`processAndDisplay()`、工具栏事件、各控制器初始化），仍可进一步拆分
4. **Magic Number**：部分算法参数（阈值、核大小、迭代次数）存在硬编码，集中在 `constants.h` 可以改善可维护性
5. **错误处理相对薄弱**：部分 OpenCV 调用缺少返回值检查和异常处理，在极端输入下可能崩溃
6. **测试缺失**：项目缺少单元测试和集成测试，算法修改后的回归验证依赖手动测试
7. ~~**HALCON 残留**：`3rdparty/halcon/` 目录仍有遗留代码引用（如 `StepLineDetect` 中的注释代码），建议清理以减少构建混淆~~ ✅ 已清理
8. **日志和配置耦合**：`spdlog` 通过 `qt_sinks.h` 自定义 QTextEdit sink 实现 UI 日志，但该文件在 `3rdparty/spdlog/` 中而非项目源码中，可能随 spdlog 更新丢失
9. **Pipeline 步骤过多**：9 个步骤在单一线程串行执行，虽然设计清晰，但对于只需要部分功能的场景仍需遍历全部步骤。可以考虑步骤注册/跳过机制
10. **跨平台局限性**：`SystemMonitor` 的 CPU/内存监控依赖 Windows PDH API，`build/Debug/bin/` 硬编码路径，对 Linux/macOS 的支持不完全

---

## 四、总结

VisionTool 是一个**架构扎实、功能完善**的机器视觉桌面应用。核心 Pipeline 模式将复杂的图像处理流程抽象为可配置的步骤链，结合 MVC 风格的控制器分离和 Qt 的异步机制，实现了良好的可扩展性和用户体验。

项目最大的亮点是**覆盖了工业视觉检测的完整闭环**：图像采集 → 预处理 → 增强/滤波 → 形态学 → 特征提取 → 测量/识别 → 判定 → MQTT 输出，这种端到端的设计使其具备实际产线部署潜力。

主要改进方向包括：引入测试体系、消除 CMake GLOB 和硬编码、以及加强错误处理。
