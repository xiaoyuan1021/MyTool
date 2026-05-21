# 项目摘要：EdgeVision — 边云协同智能视觉检测系统

## 一、项目概述

**EdgeVision** 是一款面向工业自动化产线的智能视觉检测平台，融合传统图像处理与深度学习推理能力，采用可配置化 Pipeline 引擎和 MQTT 边云协同架构，旨在为中小型企业提供低成本、易部署、可扩展的视觉质检解决方案。

---

## 二、研究背景与动机

工业视觉检测是智能制造的核心环节。据统计，制造业中约 **30%** 的质量缺陷可通过视觉检测提前发现。然而，现有视觉检测工具面临三大挑战：

1. **检测流程固化**：更换产品需重新编写检测程序，开发周期长、成本高
2. **算法能力单一**：传统图像处理工具难以应对复杂场景，深度学习工具部署门槛高
3. **数据无法远程管控**：检测结果局限于本地，无法实现实时远程监控和数据分析

EdgeVision 针对以上痛点，提出了一套**"可配置化 Pipeline + 多模态算法融合 + 边云协同"**的综合解决方案。

---

## 三、系统设计

### 3.1 总体架构

系统采用**六层分层架构**，自上而下为：UI 层 → Widgets 层 → Controllers 层 → Core/Algorithm 层 → Config/Data 层 → Cloud Platform 层。各层职责清晰，依赖关系单向，确保系统的可维护性和可扩展性。

```
┌──────────────────────────────────────────────────────────┐
│                       UI 层                               │
│   MainWindow · ImageView · Toast · SystemMonitor         │
│   (QMainWindow · 信号槽 · 图像渲染 · Toast通知 · 资源监控) │
├──────────────────────────────────────────────────────────┤
│                    Widgets 层                              │
│   StepConfigWidget · ImageTab · VideoTab · EnhanceTab    │
│   FilterTab · ProcessTab · ExtractTab · JudgeTab          │
│   LineTab · TemplateTab · BarcodeTab · ObjectDetectionTab│
│   ProfileTab · BatchDetectionWidget                       │
├──────────────────────────────────────────────────────────┤
│                  Controllers 层                            │
│   AutoDetectionController · DetectionUiController        │
│   RoiUiController · ProfileController                    │
│   ConfigController · PipelineResultHandler               │
├──────────────────┬───────────────────────────────────────┤
│     Core 层       │         Algorithm 层                   │
│   Pipeline        │   ImageProcessor · OpenCVAlgorithm    │
│   PipelineManager │   DnnInference · OrtInference (GPU)  │
│   RoiManager      │   ZXingBarcodeReader · MatchStrategy │
│   ProfileManager  │   DisplayRenderer · ImageUtils       │
│   VideoManager    │                                       │
│   MqttManager     │                                       │
├──────────────────┴───────────────────────────────────────┤
│                Config / Data 层                            │
│   PipelineConfig · DetectionItem · RoiConfig              │
│   InspectionProfile · DetectionResultReport · BarcodeResult│
│   ShapeFilterConfig · RegionFeature                       │
└──────────────────────────────────────────────────────────┘
                          │
                          ▼
┌──────────────────────────────────────────────────────────┐
│              Cloud Platform (Python Flask)                │
│   MQTT Subscriber · SQLite Persistence · SSE Push       │
│   Chart.js Statistics · Remote Command API               │
└──────────────────────────────────────────────────────────┘
```

### 3.2 核心技术

| 技术方向 | 具体实现 | 创新点 |
|----------|----------|--------|
| **可配置化 Pipeline** | 9 步处理管线，步骤可独立配置、拖拽排序 | 零代码搭建检测流程，步骤顺序自由调整，实时预览效果 |
| **多模态检测** | 传统算法 + YOLO 深度学习 + 条码识别 | 三种检测能力融合，适应多种工业场景 |
| **GPU 加速推理** | ONNX Runtime + CUDA EP | YOLOv8n 推理速度 ~15ms/帧 |
| **边云协同** | MQTT 5.0 + Flask Web 看板 + SSE实时推送 | 检测结果实时上报，云端远程监控与指令下发 |
| **检测方案持久化** | ROI 归一化 + 参数序列化 + 模板库 | 跨设备复用，一键切换产品 |
| **接口化 Tab 架构** | 4 个抽象接口解耦 Tab 与核心模块 | 新增检测类型零侵入，插件式扩展 |

### 3.3 Pipeline 引擎

Pipeline 引擎是系统的核心处理单元，采用**步骤链模式**（Chain of Responsibility），每个步骤实现 `IPipelineStep` 接口，由 `PipelineManager` 统一编排执行。

#### 3.3.1 步骤链执行模型

```
输入图像 (BGR)
    │
    ▼
┌─────────────────────┐
│  Step 1: 通道转换     │  Gray / RGB / HSV / 单通道 (B/G/R)
└─────────┬───────────┘
          ▼
┌─────────────────────┐
│  Step 2: 图像增强     │  亮度 + 对比度 + Gamma + 锐化
└─────────┬───────────┘
          ▼
┌─────────────────────┐
│  Step 3: 灰度过滤     │  灰度阈值范围过滤
└─────────┬───────────┘
          ▼
┌─────────────────────┐
│  Step 4: 颜色过滤     │  RGB / HSV 范围过滤
└─────────┬───────────┘
          ▼
┌─────────────────────┐
│  Step 5: 形态学处理    │  开闭运算 / 膨胀腐蚀 / 连通域 / 填充 / 面积筛选
└─────────┬───────────┘
          ▼
┌─────────────────────┐
│  Step 6: 形状筛选     │  面积、圆度、凸性、矩形度 (AND/OR 逻辑)
└─────────┬───────────┘
          ▼
┌─────────────────────┐
│  Step 7: 直线检测     │  HoughP / LSD / EDline
└─────────┬───────────┘
          ▼
┌─────────────────────┐
│  Step 8: 参考线匹配   │  角度 + 距离容差匹配
└─────────┬───────────┘
          ▼
┌─────────────────────┐
│  Step 9: 条码识别     │  ZXing 多格式条码 / QR Code
└─────────┬───────────┘
          ▼
输出: 区域特征 + 判定结果 + 条码数据 + 可视化叠加
```

#### 3.3.2 步骤配置与拖拽排序

`StepConfigWidget` 实现了 Pipeline 步骤的 UI 配置面板：
- **拖拽排序**：用户可通过拖拽调整步骤执行顺序，点击【应用】按钮后生效
- **步骤启用/禁用**：每个步骤可通过复选框独立开关
- **UI-后端解耦**：UI 步骤条目（`StepEntry`）与后端 `StepType` 枚举通过 `backendIndices` 映射，维护时只需增删 `kSteps` 数组
- **Tab 联动**：启用/禁用步骤时自动显示/隐藏对应的配置 Tab

#### 3.3.3 线程安全设计

- `PipelineManager::execute()` 可在后台线程调入，使用局部 `PipelineContext`，不共享可变状态
- `m_config` 仅在 UI 线程读写，无需加锁
- `m_lastContext` 由 `m_contextMutex` 互斥锁保护，供 UI 线程读取上次结果
- `m_pipelineRunning` 原子标志 + `m_hasPendingReset` 延迟重置标志，确保 Pipeline 执行与 UI 重置互不干扰
- **配置快照**：`getConfigSnapshot()` 返回配置的值拷贝，避免并发修改

#### 3.3.4 防抖刷新机制

`requestRefresh()` 提供防抖 + 脏标记机制，避免参数微调时频繁触发 Pipeline 重建：
- 短时间内多次参数变更只触发一次 `processAndDisplay()`
- `m_needRefresh` 标记确保最后一次变更不丢失

### 3.4 多模态检测能力

#### 3.4.1 传统图像处理
- **形态学操作**：圆形/矩形开闭运算、膨胀腐蚀、联合、连通域标记、填充孔洞、形状变换（凸包/最小外接矩形/拟合圆/椭圆）、面积筛选
- **形状筛选**：面积、圆度、凸性、惯性比、矩形度等多特征组合，支持 AND/OR 逻辑
- **直线检测**：HoughP / LSD / EDline 三种算法 + 参考线匹配（角度 + 距离容差）

#### 3.4.2 深度学习目标检测
- **模型支持**：ONNX / Caffe / TensorFlow / Darknet 格式
- **GPU 加速**：ONNX Runtime + CUDA Execution Provider
- **双推理后端**：视频模式使用 ONNX Runtime（GPU），静图模式可使用 OpenCV DNN（CPU/GPU）
- **可配置参数**：置信度阈值、NMS 阈值、输入分辨率、显示标签/置信度/边框

#### 3.4.3 条码识别
- **基于 ZXing-CPP**，支持 QR Code、EAN-13、Code 128、DataMatrix 等 10+ 种格式
- **图像预处理**：可选二值化或形态学预处理，提升低质量条码识别率
- **置信度过滤**：可设置最低置信度阈值

### 3.5 检测方案管理

`ProfileManager` 实现了完整的检测方案生命周期管理：

#### 3.5.1 方案结构
```
resources/profiles/<profileName>/
├── profile.json          // 方案配置（ROI布局 + 检测项 + Pipeline参数）
├── thumbnail.png         // 方案缩略图
└── templates/            // 模板库
    ├── 螺丝A.png
    ├── 螺丝A.json
    └── ...
```

#### 3.5.2 ROI 坐标归一化
- `RoiTemplate` 使用归一化坐标（0~1，相对于参考图像尺寸）保存 ROI 区域
- 加载时根据当前图像尺寸自动缩放，实现跨分辨率适配
- 每个 ROI 可挂载多个独立的 `DetectionItem`，支持同一 ROI 下同时进行多种检测

#### 3.5.3 方案管理功能
- **保存/加载/删除** 方案，支持方案列表浏览
- **模板库管理**：保存模板图像 + 匹配参数，跨图片复用
- **活跃方案跟踪**：`activeProfileId` 记录当前使用的方案
- **批量检测集成**：`BatchDetectionWidget` 可选择使用已保存方案进行批量检测

### 3.6 边云协同架构

#### 3.6.1 MQTT 通信层（`MqttClient` + `MqttManager`）

`MqttClient` 封装 Paho MQTT C++ 库，提供 Qt 信号槽接口：
- **连接管理**：自动重连（定时器 + 指数退避）、连接状态信号
- **消息收发**：支持 QoS 0/1/2、retained 消息
- **配置热更新**：`updateConfig()` 动态修改 Broker 地址并重连

`MqttManager` 实现三大核心功能：

| 功能 | 说明 | Topic |
|------|------|-------|
| **检测结果上报** | Pipeline 完成后自动发送标准化 `DetectionResultReport` | `visiontool/results` |
| **设备心跳保活** | 定时发送设备状态（运行时长、检测计数等） | `visiontool/heartbeat` |
| **云端远程指令** | 订阅云端控制指令并转发为 Qt 信号 | `visiontool/commands` |

#### 3.6.2 标准化检测报告（`DetectionResultReport`）

作为 Pipeline 结果到 MQTT 上报的数据桥梁：
- **完整 JSON 序列化**：包含元数据、检测结果、区域特征详情、条码识别结果、扩展字段
- **工厂方法**：`fromPipelineContext()` 从 Pipeline 上下文快速构造
- **摘要生成**：`toSummaryString()` 用于日志/调试

#### 3.6.3 云端平台（`Cloud Dashboard`）

Python Flask 后端（800+ 行代码），提供：
- **SQLite 持久化**：设备表、检测结果表、心跳表，WAL 模式保证并发安全
- **设备管理**：自动注册、在线/离线状态检测（心跳超时 75 秒）
- **SSE 实时推送**：Server-Sent Events 实现毫秒级数据更新到前端
- **统计分析**：合格率趋势图表、ROI 维度统计、检测量统计（Chart.js）
- **历史查询**：分页浏览、时间范围筛选
- **远程指令**：采集指令、开始/停止批量检测
- **Docker 部署**：提供 Dockerfile 和 docker-compose.yml

### 3.7 Tab 接口化架构

系统通过 4 个抽象接口实现 Tab Widget 与核心模块的解耦：

| 接口 | 职责 | 实现类 |
|------|------|--------|
| `IConfigurableTab` | 配置保存/加载（`saveToConfig` / `loadFromConfig`） | 各功能 Tab |
| `ISignalConnectable` | 信号连接（注入 PipelineManager/RoiManager 等依赖） | 各功能 Tab |
| `ITabInitializable` | 创建后初始化（注入 ProfileManager 等扩展依赖） | 需要额外初始化的 Tab |
| `IResultUpdatable` | 接收 Pipeline 结果更新（`updateFromPipeline`） | 需要显示结果的 Tab |

`TabManager` 负责 Tab 的**懒加载**（按需创建）、**类型安全获取**（`getTabAs<T>()`）和生命周期管理。
`TabRegistry` 维护 Tab 名称到工厂函数的注册表，新增 Tab 只需注册即可。

### 3.8 结果处理器（`PipelineResultHandler`）

负责 Pipeline 执行完成后的结果分发：
- **接口化分发**：通过 `IResultUpdatable` 接口向已创建的 Tab 推送结果，不再硬编码 Tab 名称
- **目标检测特殊处理**：视频帧使用 ONNX Runtime 推理，静图使用 OpenCV DNN，通过 `setVideoMode()` 切换
- **异步处理**：基于 `QFutureWatcher` 实现后台 Pipeline 执行 + 主线程结果回调

### 3.9 视频处理（`VideoManager`）

支持多种视频源的实时采集与处理：
- **视频源类型**：本地文件 / USB 摄像头
- **播放控制**：播放/暂停/停止/跳帧
- **设备枚举**：自动检测可用摄像头设备
- **帧率适配**：自动读取视频帧率，定时器匹配播放速度

---

## 四、技术栈

### 桌面端
| 技术 | 版本 | 用途 |
|------|------|------|
| **C++** | C++20 | 核心开发语言 |
| **Qt** | 6.x | 跨平台 GUI 框架，信号槽通信 |
| **OpenCV** | 4.8 | 图像处理核心库（imgproc, dnn, videoio, features2d, calib3d, ximgproc, line_descriptor 等） |
| **ONNX Runtime** | GPU | 深度学习推理引擎，CUDA 加速 |
| **ZXing-CPP** | — | 条码 / QR Code 多格式识别 |
| **Paho MQTT** | C++ | MQTT 客户端，边云协同通信 |
| **spdlog** | — | 高性能日志系统（header-only） |
| **CMake** | 3.16+ | 跨平台构建系统 |

### 云平台
| 技术 | 用途 |
|------|------|
| **Python Flask** | Web 后端框架 |
| **Paho MQTT Python** | MQTT 订阅/发布 |
| **SQLite** | 轻量级数据持久化（WAL 模式） |
| **Chart.js** | 实时统计图表 |
| **Server-Sent Events** | 服务端实时数据推送 |
| **Docker** | 容器化部署 |

---

## 五、功能特性

| # | 功能 | 说明 | 关键类/模块 |
|---|------|------|------------|
| 1 | **多图片管理** | 单张/批量导入，多图片切换，独立 ROI 配置 | `ImageListManager` |
| 2 | **ROI 管理** | 矩形绘制、多 ROI 支持、坐标归一化 | `RoiManager`, `RoiUiController` |
| 3 | **图像增强** | 亮度/对比度/Gamma/锐化，实时滑块调节 | `EnhanceTabWidget` |
| 4 | **颜色过滤** | 灰度/RGB/HSV 多模式，阈值范围过滤 | `FilterTabWidget` |
| 5 | **算法处理队列** | 10+ 种形态学操作自由组合 | `ProcessTabWidget` |
| 6 | **形状提取与筛选** | 多条件 AND/OR 逻辑，面积/圆度/凸性 | `ExtractTabWidget`, `ShapeFilterConfig` |
| 7 | **直线检测** | HoughP/LSD/EDline + 参考线匹配 | `LineTabWidget` |
| 8 | **模板匹配** | 多边形 ROI 提取，模板库管理，跨图片复用 | `TemplateTabWidget`, `ProfileManager` |
| 9 | **条码/QR Code 识别** | ZXing 多格式支持，预处理增强 | `BarcodeTabWidget`, `ZXingBarcodeReader` |
| 10 | **深度学习目标检测** | YOLO 系列 ONNX 推理，GPU 加速 | `ObjectDetectionTabWidget`, `OrtInference` |
| 11 | **视频处理** | 视频文件 + USB 摄像头实时检测 | `VideoManager`, `VideoTabWidget` |
| 12 | **批量检测** | 统一面板，支持暂停/恢复，CSV导出 | `BatchDetectionWidget` |
| 13 | **检测方案管理** | 完整方案 CRUD + 模板库，跨设备复用 | `ProfileManager`, `ProfileTabWidget` |
| 14 | **边云协同** | MQTT 结果上报 + 心跳 + 云端指令 | `MqttManager`, `MqttClient` |
| 15 | **云平台看板** | 设备监控、数据统计、历史查询、远程指令 | `Cloud Dashboard (Flask)` |
| 16 | **Pipeline 拖拽排序** | 步骤顺序自由调整，UI-后端解耦 | `StepConfigWidget` |
| 17 | **显示模式切换** | 多种可视化模式（Mask/GreenWhite/Overlay） | `DisplayModeManager` |
| 18 | **系统资源监控** | CPU/GPU 使用率实时显示 | `SystemMonitor` |
| 19 | **Toast 通知** | 操作反馈浮标，动画淡入淡出 | `ToastNotification` |
| 20 | **性能基准计时** | Pipeline 执行耗时实时显示 | `Benchmark` |

---

## 六、应用场景

| 场景 | 技术路线 | 预期效果 |
|------|----------|----------|
| **PCB 焊点检测** | 颜色过滤 + 形态学 + 面积/圆度筛选 | 自动识别虚焊、漏焊 |
| **零件尺寸测量** | 模板匹配 + 形状变换 | 非接触式尺寸测量 |
| **产品条码追溯** | ZXing 多格式条码识别 | 自动解码 + 数据比对 |
| **产线实时监控** | USB 摄像头 + YOLO + MQTT | 边缘推理 + 云端监控 |

---

## 七、性能指标（参考值）

| 指标 | 数值 | 测试环境 |
|------|------|----------|
| YOLOv8n 推理速度 | ~15ms/帧 | RTX 3060 + CUDA |
| 传统 Pipeline 延迟 | ~5ms/帧 | Intel i7-12700H |
| 条码识别成功率 | >99% (QR Code) | 标准测试图集 |
| MQTT 上报延迟 | <50ms | 本地 Broker |
| 批量检测吞吐 | ~200张/分钟 | 1920×1080 |

---

## 八、项目特色与创新

1. **可配置化 Pipeline 引擎**：将传统视觉检测的固定流程改为可自由组合、可拖拽排序的步骤链，用户无需编程即可搭建检测方案
2. **多模态算法融合**：在同一平台中融合传统图像处理、深度学习推理和条码识别三种能力
3. **边云协同架构**：基于 MQTT 5.0 协议实现边缘端与云端的实时通信，支持检测结果上报、设备心跳保活、云端远程指令下发三大功能
4. **检测方案持久化**：ROI 坐标归一化 + 完整参数序列化 + 模板库管理，实现跨设备、跨分辨率的方案复用
5. **接口化 Tab 架构**：通过 `IConfigurableTab`、`ISignalConnectable`、`ITabInitializable`、`IResultUpdatable` 四个抽象接口实现 Tab Widget 与核心模块的完全解耦，新增检测类型零侵入
6. **工业级线程安全设计**：互斥锁 + 原子标志 + 值拷贝交换 + 配置快照，确保 Pipeline 后台执行与 UI 响应互不干扰
7. **标准化检测报告**：`DetectionResultReport` 作为 Pipeline 结果到 MQTT 上报的统一数据桥梁，支持完整 JSON 序列化

---

## 九、代码统计

| 指标 | 数值 |
|------|------|
| C++ 源文件（src/） | 42 个 .cpp 文件 |
| C++ 头文件（include/） | 72 个 .h 文件 |
| Qt UI 文件（ui/） | 3 个 .ui 文件 |
| Python 云平台 | 800+ 行（app.py） |
| 第三方库 | OpenCV, ONNX Runtime, Paho MQTT, spdlog, ZXing-CPP |

### 源代码分层统计

| 层 | 源文件数 | 头文件数 | 说明 |
|----|---------|---------|------|
| Core | 10 | 10 | Pipeline 引擎、MQTT、方案管理、视频管理 |
| Algorithm | 8 | 8 | 图像处理、DNN推理、条码识别、模板匹配 |
| Config | 5 | 13 | 配置结构体、常量、序列化 |
| Data | 1 | 4 | 数据模型（方案、报告、特征、条码结果） |
| UI | 7 | 9 | 主窗口、图像视图、系统监控、Toast |
| Widgets | 20 | 22 | 各功能 Tab、图片列表管理、批量检测 |
| Controllers | 6 | 6 | 批量检测、ROI交互、配置管理、方案管理 |
| Utils | — | 2 | 路径工具、性能基准 |

---

## 十、总结

EdgeVision 通过可配置化 Pipeline 引擎、多模态算法融合和边云协同架构，为工业视觉检测提供了一套完整的解决方案。系统具有**低成本、易配置、可扩展**的特点，采用接口化架构设计，新增检测类型只需实现接口即可零侵入集成，能够有效降低视觉检测的部署门槛，提升产线质检效率。

---

*项目开源地址：https://github.com/xiaoyuan1021/MyTool*