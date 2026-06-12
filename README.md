# EdgeVision — 边云协同智能视觉检测系统

<p align="center">
  <img src="resources/icons/keji.png" width="120" alt="EdgeVision Logo">
</p>

<p align="center">
  <strong>面向工业产线的可配置化智能视觉检测平台</strong>
</p>

<p align="center">
  <img src="https://img.shields.io/badge/C%2B%2B-20-00599C?logo=c%2B%2B" alt="C++20">
  <img src="https://img.shields.io/badge/Qt-6-41CD52?logo=qt" alt="Qt6">
  <img src="https://img.shields.io/badge/OpenCV-4.8-5C3EE8?logo=opencv" alt="OpenCV4">
  <img src="https://img.shields.io/badge/ONNX Runtime-GPU-005CED?logo=nvidia" alt="ONNX Runtime">
  <img src="https://img.shields.io/badge/MQTT-5.0-66006C?logo=eclipse-iot" alt="MQTT">
  <img src="https://img.shields.io/badge/License-MIT-yellow.svg" alt="License">
</p>

---

## 📖 项目背景

在工业自动化产线中，**视觉检测**是保障产品质量的关键环节。传统视觉检测工具存在以下痛点：

| 痛点 | 现状 | EdgeVision 的解法 |
|------|------|-------------------|
| **检测流程固定** | 更换产品需重新编程 | ✅ 可配置化 Pipeline，拖拽组合检测步骤 |
| **算法单一** | 传统算法或深度学习二选一 | ✅ 传统算法 + YOLO 深度学习 + 条码识别 + OCR 融合 |
| **部署成本高** | 需要工业相机 + 专用工控机 | ✅ 普通 PC + USB 摄像头即可运行 |
| **数据孤岛** | 检测结果无法远程监控 | ✅ MQTT 边云协同，云端实时看板 |
| **GPU 利用率低** | 深度学习推理未加速 | ✅ ONNX Runtime + CUDA GPU 加速推理 |
| **文字识别困难** | 手动录入产品信息 | ✅ OCR 自动识别中英文文字 |

EdgeVision 旨在打造一个**低成本、易配置、可扩展**的智能视觉检测平台，让中小型企业也能快速部署视觉质检方案。

---

## ✨ 核心亮点

### 🧩 可配置化 Pipeline 引擎

- **12 大处理步骤**自由组合，零代码搭建检测流程
- **拖拽排序**：用户可通过拖拽调整步骤执行顺序，点击【应用】后实时生效
- 每个步骤独立配置，支持实时预览效果
- 支持保存/加载检测方案，不同产品一键切换
- **智能调度器**：PipelineScheduler 实现请求消抖合并、去重过滤、优先级队列

### 🧠 多模态检测能力

| 检测能力 | 技术方案 | 说明 |
|----------|----------|------|
| **传统图像处理** | OpenCV 形态学 | 开闭运算/膨胀腐蚀/连通域/面积筛选/形状变换 |
| **深度学习推理** | YOLO + ONNX Runtime | 双推理后端：视频用 ONNX Runtime (GPU)，静图用 OpenCV DNN |
| **条码/QR Code** | ZXing-CPP | 支持 10+ 种条码格式，图像预处理增强 |
| **直线检测** | HoughP / LSD / EDline | 三种算法 + 参考线匹配（角度 + 距离容差） |
| **OCR 文字识别** | RapidOCR PP-OCRv4 | 支持中英文识别，启动时自动预热模型 |
| **模板匹配** | OpenCV matchTemplate | 多边形 ROI 提取，批量匹配 + CSV 导出 |

### ☁️ 边云协同架构

```
┌─────────────┐        MQTT Broker        ┌──────────────────────┐
│  EdgeVision │◄──── (MQTT 5.0) ────────►│  Cloud Dashboard     │
│  桌面端 C++ │                            │  Flask Web 看板      │
│             │  检测结果实时上报            │  实时 SSE 推送       │
│  ● GPU推理  │  设备心跳保活              │  合格率趋势统计      │
│  ● ROI管理  │  云端远程指令              │  历史数据查询        │
│  ● Pipeline │  自动重连                  │  设备状态监控        │
└─────────────┘                           │  Docker 一键部署     │
                                          └──────────────────────┘
```

### 🎯 工业级特性

- **检测方案持久化**：ROI 布局 + 检测参数 + 模板库完整保存，支持跨设备复用
- **批量检测**：自动遍历多张图片，实时统计合格率，不合格图片标记导出
- **视频流处理**：支持视频文件和 USB 摄像头实时检测
- **ROI 管理**：多 ROI 独立配置，坐标归一化适配不同分辨率
- **线程安全**：互斥锁 + 原子标志 + 配置快照，Pipeline 后台执行不阻塞 UI
- **启动预热**：OCR 和条码引擎在应用启动时自动预热，消除首次延迟

---

## 🏗️ 系统架构

项目采用 **六层分层架构**，职责清晰，依赖单向：

```
┌──────────────────────────────────────────────────────────┐
│                       UI 层                               │
│   MainWindow · ImageView · Toast · SystemMonitor         │
│   DisplayModeManager · CloudDashboardManager              │
├──────────────────────────────────────────────────────────┤
│                    Widgets 层                              │
│   StepConfigWidget (拖拽排序) · ImageFilterTab            │
│   ImageTab · VideoTab · EnhanceTab · FilterTab           │
│   ProcessTab · ExtractTab · JudgeTab · LineTab            │
│   TemplateTab · BarcodeTab · ObjectDetectionTab          │
│   ProfileTab · BatchDetectionWidget · OcrTab             │
├──────────────────────────────────────────────────────────┤
│                  Controllers 层                            │
│   AutoDetectionController · DetectionUiController        │
│   RoiUiController · RoiDetectionConfigController         │
│   ProfileController · ConfigController                   │
│   PipelineResultHandler (接口化结果分发)                   │
├──────────────────┬───────────────────────────────────────┤
│     Core 层       │         Algorithm 层                   │
│   Pipeline        │   ImageProcessor · OpenCVAlgorithm    │
│   PipelineManager │   DnnInference · OrtInference (GPU)  │
│   PipelineScheduler│  ZXingBarcodeReader · MatchStrategy │
│   RoiManager      │   RapidOcrEngine                      │
│   ProfileManager  │   DetectionEvaluator · DisplayRenderer│
│   VideoManager    │                                       │
│   MqttManager     │                                       │
├──────────────────┴───────────────────────────────────────┤
│                Config / Data 层                            │
│   PipelineConfig · DetectionItem · RoiConfig              │
│   InspectionProfile · DetectionResultReport              │
│   RoiDetectionResult · ImageDetectionResult              │
│   ShapeFilterConfig · RegionFeature · BarcodeResult      │
│   OcrConfig · OcrRegion · DisplayConfig                  │
└──────────────────────────────────────────────────────────┘
                          │
                          ▼
┌──────────────────────────────────────────────────────────┐
│           Cloud Platform (Python Flask + EMQX)            │
│   MQTT Handler · SQLite (WAL) · SSE Push                 │
│   Chart.js Statistics · REST API · Docker Compose        │
└──────────────────────────────────────────────────────────┘
```

---

## 🔧 技术栈

### 桌面端 (C++20)

| 技术 | 版本 | 用途 |
|------|------|------|
| **C++20** | — | 核心开发语言，利用 concepts/ranges 等现代特性 |
| **Qt 6** | 6.x | 跨平台 GUI 框架，信号槽通信机制 |
| **OpenCV** | 4.8 | 图像处理核心库（imgproc, dnn, videoio, features2d, calib3d, ximgproc, line_descriptor） |
| **ONNX Runtime** | GPU | 深度学习推理引擎，CUDA GPU 加速 YOLO 目标检测 |
| **ZXing-CPP** | — | 条码 / QR Code 多格式识别 |
| **RapidOCR** | PP-OCRv4 | OCR 文字识别引擎，支持中英文 |
| **Paho MQTT** | 5.0 | MQTT 客户端，边云协同通信 |
| **spdlog** | — | 高性能日志系统（header-only，std::format） |
| **CMake** | 3.16+ | 跨平台构建系统（Ninja 生成器） |

### 云平台 (Python)

| 技术 | 用途 |
|------|------|
| **Flask** | Web 后端框架（waitress 生产服务器） |
| **Paho MQTT** | MQTT 订阅/发布 |
| **SQLite** | 轻量级数据持久化（WAL 模式） |
| **Chart.js** | 实时统计图表（趋势线 + ROI 饼图） |
| **Server-Sent Events** | 服务端毫秒级数据推送 |
| **Docker + EMQX 5.8** | 容器化部署，一键启动 MQTT Broker + Dashboard |

---

## 📸 效果展示

### 桌面端主界面
![主界面](resources/screenshots/MainWindow.png)

### 传统 Blob 分析（斑点搜寻）
![Blob](resources/screenshots/Blob.png)

### 深度学习目标检测
![YOLO检测](resources/screenshots/pin.png)

### 条码识别
![条码识别](resources/screenshots/barcode.png)

### OCR 字符识别
![OCR](resources/screenshots/OCR.png)

### 云平台看板
![云平台](resources/screenshots/kanban.png)

---

## 📊 性能指标

| 指标 | 数值 | 测试环境 |
|------|------|----------|
| 传统 Blob 分析 | ~30ms/帧 | Intel i5-9300H |
| QR Code 识别 | ~100ms/帧 | Intel i5-9300H + ZXing |
| 条形码识别 | ~200ms/帧 | Intel i5-9300H + ZXing |
| 目标检测 (YOLOv8n) | ~300ms/帧 (首次 ~700ms 含初始化) | Intel i5-9300H CPU 推理 |
| OCR 文字识别 | **~60ms/帧** (首次 ~400ms 含初始化) | Intel i5-9300H + RapidOCR PP-OCRv4 |

---

## 🚀 快速开始

### 系统要求

- **操作系统**：Windows 10/11 (x64)
- **编译器**：Visual Studio 2022 (MSVC，支持 C++20)
- **构建工具**：CMake 3.16+（Ninja 生成器）
- **Qt**：6.x
- **Python**：3.8+（云平台看板）
- **GPU**（可选）：NVIDIA GPU + CUDA Toolkit（用于深度学习 GPU 加速）

### 编译运行

```powershell
# 1. 克隆仓库
git clone https://github.com/xiaoyuan1021/MyTool.git
cd MyTool

# 2. 首次编译 ZXing（仅需一次）
mkdir 3rdparty\zxing\lib
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"
cmake -S 3rdparty/zxing/zxing-cpp -B build_zxing_debug -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build_zxing_debug --config Debug
copy build_zxing_debug\core\lib\ZXingd.lib 3rdparty\zxing\lib\ZXingd.lib

# 3. 编译主程序
mkdir build\Debug
cd build\Debug
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"
cmake -G Ninja -DCMAKE_BUILD_TYPE=Debug ../..
ninja

# 4. 运行
cd bin
.\EdgeVision.exe
```

### 云平台启动

```bash
# 方式一：直接运行
cd cloud_dashboard
pip install -r requirements.txt
python app.py
# 浏览器访问 http://localhost:5000

# 方式二：Docker 一键部署（含 EMQX MQTT Broker）
cd cloud_dashboard
docker compose up --build
```

---

## 📁 项目结构

```
EdgeVision/
├── 3rdparty/                    # 第三方库（vendored）
│   ├── opencv/                  # OpenCV 4.x（含 ximgproc, line_descriptor 等模块）
│   ├── onnxruntime/             # ONNX Runtime GPU（CUDA 加速推理）
│   ├── paho-mqtt/               # Paho MQTT C++ 客户端
│   ├── spdlog/                  # 高性能日志库（header-only）
│   ├── zxing/                   # ZXing-CPP 条码识别
│   └── rapidocr/               # RapidOCR PP-OCRv4 ONNX 模型
│
├── include/                     # 头文件
│   ├── algorithm/               # 算法层：图像处理、DNN/ONNX推理、条码、OCR、检测评估、渲染
│   ├── config/                  # 配置层：Pipeline、ROI、增强、过滤、直线、条码、OCR 等 21 个配置头
│   ├── controllers/             # 控制器层：批量检测、ROI交互、配置管理、Pipeline结果分发
│   ├── core/                    # 核心层：Pipeline引擎、调度器、MQTT、方案管理、视频管理
│   ├── data/                    # 数据层：检测报告、ROI检测结果、区域特征、条码/OCR结果
│   ├── ui/                      # UI组件：主窗口、图像视图、Toast、系统监控、云端看板管理
│   ├── utils/                   # 工具类：性能基准、模型预热、路径工具
│   └── widgets/                 # Widget层：23 个功能组件（含步骤拖拽、图像过滤、批量匹配等）
│
├── src/                         # 源文件
│   ├── algorithm/               # 算法实现
│   ├── config/                  # 配置实现
│   ├── controllers/             # 控制器实现
│   ├── core/                    # 核心引擎实现（含Pipeline调度器）
│   ├── ui/                      # UI组件实现
│   ├── utils/                   # 工具类实现
│   ├── widgets/                 # Widget实现
│   └── main.cpp                 # 程序入口
│
├── ui/                          # Qt Designer UI 文件
│   ├── mainwindow.ui            # 主窗口布局
│   ├── home_page.ui             # 系统概览页
│   ├── log_page.ui              # 日志页
│   └── tabs/                    # 13 个功能 Tab 的 UI 布局
│
├── resources/                   # 资源文件
│   ├── icons/                   # 图标资源
│   ├── images/                  # 测试图片（PCB、条码、pin 针、行人检测视频等）
│   ├── models/                  # 深度学习模型（YOLOv8n ONNX）
│   ├── screenshots/             # README 截图
│   ├── template/                # 模板匹配图像
│   ├── ocr_models/               # RapidOCR PP-OCRv4 ONNX 模型
│   ├── style.qss                # 全局样式表（科技蓝渐变主题）
│   └── res.qrc                  # Qt 资源文件
│
├── cloud_dashboard/             # 云平台看板 (Python Flask)
│   ├── app.py                   # Flask 入口 + REST API + SSE + 调试接口
│   ├── db.py                    # SQLite 持久化层（WAL 模式）
│   ├── mqtt_handler.py          # MQTT 回调与设备/结果处理
│   ├── config.py                # 云端配置（MQTT Broker、数据库路径）
│   ├── Dockerfile               # Docker 镜像
│   ├── docker-compose.yml       # Docker Compose（EMQX 5.8 + Flask Dashboard）
│   ├── static/
│   │   ├── app.js               # 前端逻辑（Chart.js 图表 + SSE 连接）
│   │   └── style.css            # 前端样式
│   ├── templates/
│   │   └── index.html           # 仪表盘页面（暗色主题）
│   └── requirements.txt         # Python 依赖
│
├── logs/                        # 运行日志
├── CMakeLists.txt               # CMake 构建配置
├── app_config.json              # 应用默认配置
├── AGENTS.md                    # 开发规范文档
└── README.md                    # 项目说明
```

---

## 🔄 处理管线 (Pipeline)

Pipeline 是 EdgeVision 的核心引擎，采用**可配置步骤链模式**，每个步骤实现 `IPipelineStep` 接口，由 `PipelineManager` 统一编排执行。

### 步骤链流程

```
输入图像 (BGR)
    │
    ▼
┌─────────────────────┐
│  Step 1: 颜色通道     │  Gray / RGB / HSV / 单通道 (B/G/R)
└─────────┬───────────┘
          ▼
┌─────────────────────┐
│  Step 2: 图像增强     │  亮度 + 对比度 + Gamma + 锐化
└─────────┬───────────┘
          ▼
┌─────────────────────┐
│  Step 3: 颜色过滤     │  灰度/RGB/HSV 范围过滤
└─────────┬───────────┘
          ▼
┌─────────────────────┐
│  Step 4: 算法处理     │  形态学操作（开闭运算/膨胀腐蚀/连通域/填充/面积筛选）
└─────────┬───────────┘
          ▼
┌─────────────────────┐
│  Step 5: 形状筛选     │  面积、圆度、凸性、矩形度（AND/OR 逻辑）
└─────────┬───────────┘
          ▼
┌─────────────────────┐
│  Step 6: 直线检测     │  HoughP / LSD / EDline + 参考线匹配
└─────────┬───────────┘
          ▼
┌─────────────────────┐
│  Step 7: 条码识别     │  ZXing 多格式条码 / QR Code
└─────────┬───────────┘
          ▼
┌─────────────────────┐
│  Step 8: 滤波去噪     │  高斯/中值/双边/形态学滤波
└─────────┬───────────┘
          ▼
┌─────────────────────┐
│  Step 9: OCR文字识别  │  Tesseract OCR（中英文识别）
└─────────┬───────────┘
          ▼
┌─────────────────────┐
│  Step 10: 模板匹配    │  多边形 ROI 提取 + matchTemplate
└─────────┬───────────┘
          ▼
┌─────────────────────┐
│  Step 11: 目标检测    │  YOLO 深度学习推理（ONNX Runtime GPU）
└─────────┬───────────┘
          ▼
输出: 区域特征 + 判定结果 + 可视化叠加
```

> **注意**：以上步骤均为**可选**，用户可通过 `StepConfigWidget` 拖拽排序 + 勾选启用/禁用。实际执行顺序由 `stepOrder` 数组决定。

### 形态学处理步骤

| 算法 | 说明 | 典型应用 |
|------|------|----------|
| 圆形开/闭运算 | 圆形结构元素形态学操作 | 去噪 / 填充小孔 |
| 矩形开/闭运算 | 矩形结构元素形态学操作 | 去除线性噪声 |
| 圆形膨胀/腐蚀 | 扩张/收缩前景区域 | 断开粘连 / 增强特征 |
| 联合 (Union) | 合并所有连通域 | 区域合并 |
| 连通域 (Connection) | 连通域标记与分析 | 区域计数 |
| 填充孔洞 (FillUp) | 填充二值图像中的孔洞 | 完善目标区域 |
| 形状变换 (ShapeTrans) | 凸包、最小外接矩形、拟合圆/椭圆 | 形状规范化 |
| 面积筛选 (SelectShape) | 按面积范围筛选连通域 | 去除干扰区域 |

### Pipeline 调度器

`PipelineScheduler` 统一管理所有 Pipeline 执行请求：

- **消抖合并**：短时间内多个请求只执行最后一个（默认 100ms 窗口）
- **去重过滤**：相同配置的请求不重复执行
- **优先级队列**：高优先级请求（如用户手动触发）优先执行
- **异步执行**：基于 `QThreadPool` + `QFutureWatcher`，不阻塞 UI

### 线程安全设计

- `PipelineManager::execute()` 使用局部 `PipelineContext`，不共享可变状态
- `m_config` 仅在 UI 线程读写，无需加锁
- `m_lastContext` 由 `m_contextMutex` 互斥锁保护
- `m_pipelineRunning` 原子标志 + `m_hasPendingReset` 延迟重置标志
- **配置快照**：`getConfigSnapshot()` 返回值拷贝，避免并发修改

### 检测结果体系

```
ImageDetectionResult（图片级）
  └── RoiDetectionResult（ROI 级）× N
        ├── DetectionItemResult（检测项级）× M
        │     ├── Blob 分析
        │     ├── 条码识别
        │     ├── 直线检测
        │     ├── OCR 识别
        │     └── 目标检测
        ├── regionFeatures（区域特征）
        ├── barcodeResults（条码结果）
        └── ocrRegions（OCR 结果）
```

---

## ☁️ 边云协同

### 通信协议

EdgeVision 使用 **MQTT 5.0** 协议实现边云通信：

| Topic | 方向 | 用途 |
|-------|------|------|
| `visiontool/results` | 边缘 → 云端 | 检测结果实时上报（DetectionResultReport JSON） |
| `visiontool/heartbeat` | 边缘 → 云端 | 设备心跳保活（运行时长 + 检测计数） |
| `visiontool/commands` | 云端 → 边缘 | 远程控制指令（采集/启停/Ping） |

### 云端平台 API

| 接口 | 方法 | 说明 |
|------|------|------|
| `/api/status` | GET | MQTT 连接状态 + 设备数 + 结果数 |
| `/api/stats` | GET | 全量统计（总数/合格/不合格/合格率） |
| `/api/stats/today` | GET | 今日统计 |
| `/api/stats/by-roi` | GET | 按 ROI 维度统计 |
| `/api/stats/by-hour` | GET | 按小时维度统计 |
| `/api/devices` | GET | 设备列表（在线/离线状态） |
| `/api/results` | GET | 最近检测结果 |
| `/api/results/history` | GET | 历史数据（支持分页/时间筛选） |
| `/api/command` | POST | 发送远程指令 |
| `/stream` | GET | SSE 实时数据推送 |
| `/api/simulate` | POST | 模拟数据生成（调试用） |

### 云平台功能

- **设备状态监控**：在线/离线状态、心跳超时自动标记（75 秒）
- **实时数据推送**：基于 SSE (Server-Sent Events) 的毫秒级数据更新
- **统计分析**：合格率趋势图 + ROI 维度饼图 + 检测量统计（Chart.js）
- **历史数据查询**：分页浏览、设备/ROI/结果状态/时间范围多维筛选
- **远程指令**：采集、开始/停止批量检测、Ping 测试
- **Docker 部署**：`docker compose up --build` 一键启动 EMQX 5.8 Broker + Flask Dashboard

---

## 🎯 应用场景

### 场景一：PCB 焊点检测
- 利用颜色过滤 + 形态学处理识别焊点区域
- 通过面积筛选和圆度判定焊点质量
- 批量检测多张 PCB 图片，统计合格率

### 场景二：零件尺寸测量
- 利用模板匹配定位零件位置
- 通过形状变换（最小外接矩形、拟合圆）提取尺寸特征
- 判定尺寸是否在公差范围内

### 场景三：产品条码追溯
- 自动识别产品上的 QR Code / 条形码
- 解码内容与数据库比对，验证产品信息
- 支持旋转、模糊、低对比度等复杂场景

### 场景四：产线实时监控
- USB 摄像头实时采集 + YOLO 目标检测
- 检测结果通过 MQTT 实时上报云端
- 云平台看板远程监控产线状态

### 场景五：OCR 文字识别
- 自动识别产品包装上的文字信息
- 支持中英文混合识别
- 适用于生产日期、批号、序列号等文字检测

### 场景六：批量模板匹配
- 批量遍历多张图片执行模板匹配
- 支持多边形 ROI 提取、模板库管理
- 结果表格展示 + 双击查看详情 + CSV 导出

---

## 🛠️ 扩展指南

### 新增 Pipeline 步骤

1. 在 `include/core/pipeline_steps.h` 中实现 `IPipelineStep` 子类
2. 在 `src/core/pipeline_steps.cpp` 中实现 `run()` 方法
3. 在 `include/config/pipeline_config.h` 的 `StepType` 枚举中添加新类型
4. 在 `PipelineManager::initPipeline()` 中注册步骤
5. 在 `StepConfigWidget::kSteps[]` 中添加 UI 步骤条目

### 新增检测类型

1. 在 `include/config/detection_type.h` 中添加 `DetectionType` 枚举值
2. 在 `include/config/detection_config_types.h` 中添加配置结构体
3. 在 `DetectionItem` 构造函数中初始化默认配置
4. 在 `DetectionEvaluator` 中添加评估逻辑
5. 创建对应的 Tab Widget，实现 `ISignalConnectable` + `IConfigurableTab` + `IResultUpdatable` 接口

### 新增 Tab Widget

1. 创建 Widget 类，继承 `QWidget` 并实现 `ISignalConnectable` 和 `IConfigurableTab` 接口
2. 在 `TabRegistry` 中注册工厂函数
3. 在 `StepConfigWidget::kSteps[]` 中关联到对应步骤（可选）

### 设计原则

| 原则 | 实现方式 |
|------|----------|
| **解耦** | 4 个抽象接口（`IConfigurableTab`、`ISignalConnectable`、`ITabInitializable`、`IResultUpdatable`）解耦 Tab 与核心模块 |
| **单职** | MainWindow 通过组合 Manager/Controller 编排功能，每个 Controller 负责单一职责 |
| **可扩展** | 新增检测类型只需枚举 + 配置结构体 + Tab Widget + 评估逻辑 |
| **线程安全** | 互斥锁 + 原子标志 + 配置快照 + 值拷贝交换 |
| **数据与渲染分离** | `PipelineContext` 存储数据，`DisplayRenderer` 负责渲染，11 种显示模式自由切换 |

---

## 📄 许可证

本项目基于 [MIT 许可证](LICENSE) 开源。

---

<p align="center">
  <strong>EdgeVision</strong> — 让智能视觉检测触手可及
</p>
