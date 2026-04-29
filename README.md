# EdgeVision — 边云协同智能视觉检测工具

[![C++20](https://img.shields.io/badge/C%2B%2B-20-00599C?logo=c%2B%2B)](https://en.cppreference.com/w/cpp/20)
[![Qt6](https://img.shields.io/badge/Qt-6-41CD52?logo=qt)](https://www.qt.io/)
[![OpenCV4](https://img.shields.io/badge/OpenCV-4.8-5C3EE8?logo=opencv)](https://opencv.org/)
[![CMake](https://img.shields.io/badge/CMake-3.16+-064F8C?logo=cmake)](https://cmake.org/)
[![License](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)

---

**EdgeVision** 是一款基于 Qt6 + OpenCV 的桌面端智能视觉检测工具，融合传统图像处理与深度学习推理能力，支持 MQTT 边云协同，适用于工业自动化产线中的视觉检测场景。

---

## 目录

- [技术栈](#技术栈)
- [系统架构](#系统架构)
- [核心功能](#核心功能)
- [处理管线](#处理管线)
- [界面预览](#界面预览)
- [快速开始](#快速开始)
- [项目结构](#项目结构)
- [边云协同](#边云协同)
- [开发指南](#开发指南)
- [许可证](#许可证)

---

## 技术栈

### 桌面端 (C++)

| 技术 | 用途 |
|------|------|
| **C++20** | 核心开发语言 |
| **Qt 6** (Core / Gui / Widgets) | 跨平台 GUI 框架，信号槽通信 |
| **OpenCV 4.x** | 图像处理核心库（imgproc、dnn、videoio、features2d、calib3d 等） |
| **ZXing-CPP** | 条码 / QR Code 识别 |
| **Paho MQTT C++** | MQTT 客户端，边云通信 |
| **spdlog** | 高性能日志系统 |
| **CMake** | 构建系统 |

### 云平台 (Python)

| 技术 | 用途 |
|------|------|
| **Flask** | Web 后端框架 |
| **Paho MQTT Python** | MQTT 订阅/发布 |
| **SQLite** | 数据持久化 |
| **Chart.js** | 实时图表展示 |
| **Server-Sent Events (SSE)** | 实时数据推送 |

---

## 系统架构

项目采用**分层 + 模块化**架构，各层职责清晰，依赖关系单向。

```
┌────────────────────────────────────────────────────────┐
│                     UI 层                                │
│  MainWindow  │  ImageView  │  TabManager  │  ...         │
│  (QMainWindow, 信号槽事件处理, 图像显示, 交互)              │
├────────────────────────────────────────────────────────┤
│                   Widgets 层                             │
│  ImageTab  │  VideoTab  │  EnhanceTab  │  FilterTab     │
│  ProcessTab│  ExtractTab│  JudgeTab    │  LineTab       │
│  TemplateTab│ BarcodeTab│ ObjectDetect │  BatchDetect   │
│  ProfileTab│  RoiListWidget  │  ImageListManager        │
├────────────────────────────────────────────────────────┤
│                Controllers 层                            │
│  AutoDetectionController  │  RoiUiController            │
│  DetectionUiController    │  ConfigController           │
│  ProfileController                                      │
├────────────────────────┬───────────────────────────────┤
│      Core 层            │      Algorithm 层              │
│  Pipeline               │  ImageProcessor               │
│  PipelineManager        │  OpenCVAlgorithm              │
│  PipelineSteps          │  DnnInference (YOLO)          │
│  RoiManager             │  ZXingBarcodeReader           │
│  ProfileManager         │  MatchStrategy                │
│  VideoManager           │                               │
│  MqttManager            │                               │
├────────────────────────┴───────────────────────────────┤
│               Config / Data 层                           │
│  PipelineConfig  │  DetectionItem  │  RoiConfig          │
│  ShapeFilterConfig│ InspectionProfile│ DetectionResult   │
│  RegionFeature   │  BarcodeResult  │  DisplayConfig     │
└────────────────────────────────────────────────────────┘
                         │
                         ▼
┌────────────────────────────────────────────────────────┐
│               Cloud Platform (Python)                  │
│  Flask Web Dashboard  │  MQTT Subscriber               │
│  SQLite Persistence   │  SSE Real-time Push             │
│  Chart.js Statistics  │  Remote Command API             │
└────────────────────────────────────────────────────────┘
```

### 各层职责

- **UI 层**：继承自 `QMainWindow`，负责事件处理、图像显示、模块协调。通过 `ISignalConnectable` 接口实现 Tab 与核心模块的解耦连接。
- **Widgets 层**：各个功能 Tab 的独立 Widget，每个 Tab 封装一个功能维度。通过 `SignalConnector` 统一建立信号连接。
- **Controllers 层**：将业务逻辑从 UI 中抽离，承担批量检测控制、UI 交互协调、配置管理等职责。
- **Core 层**：核心处理引擎，包括图像处理管线 Pipeline、ROI 管理、检测方案管理、视频管理、MQTT 通信管理等。
- **Algorithm 层**：算法封装，包括 OpenCV 传统图像处理算法、DNN 深度学习推理、ZXing 条码识别、模板匹配策略等。
- **Config / Data 层**：配置参数和数据结构定义，所有配置支持 JSON 序列化，方便持久化和网络传输。
- **Cloud Platform 层**：Python Flask 独立进程，通过 MQTT 与桌面端通信，提供 Web 可视化看板。

---

## 核心功能

### 1. 多图片管理
- 加载单张图片或批量从文件夹导入
- 多图片切换，每张图片独立维护 ROI 配置和检测参数
- 支持常见图像格式（JPG、PNG、BMP、TIFF 等）

### 2. 感兴趣区域 (ROI) 管理
- 在图像上绘制矩形 ROI 区域
- 支持多 ROI （每个 ROI 独立配置检测项和处理参数）
- ROI 区域选中、切换、删除、重置
- ROI 配置可保存到检测方案

### 3. 图像增强
- 亮度 / 对比度调节
- Gamma 校正
- 锐化处理

### 4. 颜色过滤
- 灰度阈值过滤
- RGB 三通道范围过滤
- HSV 色彩空间过滤
- 多种过滤模式切换

### 5. 算法处理队列
可组合、可排序的图像处理算法步骤：

| 算法 | 说明 |
|------|------|
| 圆形开/闭运算 | 形态学操作，圆形结构元素 |
| 矩形开/闭运算 | 形态学操作，矩形结构元素 |
| 圆形膨胀/腐蚀 | 形态学操作，圆形结构元素 |
| 矩形膨胀/腐蚀 | 形态学操作，矩形结构元素 |
| 联合 (Union) | 合并所有连通域 |
| 连通域 (Connection) | 连通域标记 |
| 填充孔洞 (FillUp) | 填充二值图像中的孔洞 |
| 形状变换 (ShapeTrans) | 凸包、最小外接矩形、拟合圆/椭圆 |
| 面积筛选 (SelectShape) | 按面积范围筛选连通域 |

### 6. 形状提取与筛选
- 使用轮廓分析提取区域特征
- 支持多条件组合筛选（AND/OR 逻辑）
- 可筛选特征：面积、圆度、凸性、矩形度、惯性比

### 7. 直线检测
- 三种算法：HoughP、LSD、EDline
- 可调参数：阈值、最小长度、最大间隙
- 参考线匹配：指定参考线，搜索匹配的角度和距离容差

### 8. 模板匹配
- 基于 OpenCV 模板匹配（支持多种匹配方法）
- 多边形 ROI 提取模板
- 模板保存到检测方案，支持跨图片复用

### 9. 条码 / QR Code 识别
- 基于 ZXing-CPP 库
- 支持多种条码格式：QR Code、Data Matrix、EAN-13、Code 128 等
- 支持旋转、反转、降采样尝试识别

### 10. 深度学习目标检测
- 基于 OpenCV DNN 模块
- 支持 ONNX / Caffe / TensorFlow / Darknet 模型
- YOLO 系列模型推理
- 置信度阈值、NMS 阈值可调

### 11. 视频处理
- 支持视频文件和摄像头输入
- 播放速度控制、帧跳跃
- 逐帧 Pipeline 处理

### 12. 批量检测
- 自动遍历多张图片或多 ROI 执行 Pipeline
- 实时进度、合格率统计
- 不合格图片标记与结果导出
- 暂停 / 继续 / 停止控制

### 13. 检测方案 (Profile)
- 将 ROI 布局、检测项配置、Pipeline 参数和模板库打包为检测方案
- 方案持久化到磁盘（profile.json + 模板图像 + 参数文件）
- ROI 坐标归一化，支持不同分辨率图像复用

### 14. 多种显示模式
- 原图 / 通道图 / 增强图
- Mask 绿白显示 / Mask 叠加显示
- 算法处理结果 / 直线检测结果
- 透明度调节

### 15. 边云协同 (MQTT)
- 检测结果实时上报到云平台
- 设备心跳保活（可配置间隔）
- 云端远程指令：采集、启动/停止检测
- 自动重连机制

### 16. 云平台看板
- 基于 Flask 的 Web 仪表盘，独立进程运行
- 设备在线状态监控
- 检测结果实时推送（SSE）
- 合格率统计与图表展示
- ROI 维度统计
- 历史数据查询与分页
- 模拟数据生成

### 17. 系统监控与日志
- CPU / 内存使用率实时监控
- spdlog 高性能日志
- Toast 通知浮标

---

## 处理管线

Pipeline 是图像处理的核心引擎，由多个可配置步骤串联执行：

```
输入图像 (BGR)
    │
    ▼
Step 1: 颜色通道转换 ─── Gray / RGB / HSV / 单通道
    │
    ▼
Step 2: 图像增强 ─── 亮度 + 对比度 + Gamma + 锐化
    │
    ▼
Step 3: 灰度过滤 ─── 灰度阈值范围过滤
    │
    ▼
Step 4: 颜色过滤 ─── RGB 或 HSV 范围过滤
    │
    ▼
Step 5: 算法队列 ─── 形态学操作 / 连通域 / 填充 / 形状变换
    │
    ▼
Step 6: 形状筛选 ─── 按面积、圆度等特征（AND/OR 逻辑）
    │
    ▼
Step 7: 直线检测 ─── HoughP / LSD / EDline
    │
    ▼
Step 8: 参考线匹配 ─── 角度 + 距离容差匹配
    │
    ▼
Step 9: 条码识别 ─── ZXing 多格式条码 / QR 识别
    │
    ▼
输出: 区域特征 + 判定结果 + 可视化
```

管线通过 `PipelineManager` 管理，支持配置的线程安全读写，后台线程异步执行。

---

## 快速开始

### 系统要求

- Windows 10/11 (x64)
- CMake 3.16+
- Visual Studio 2022 (或支持 C++20 的编译器)
- Qt 6.x
- Python 3.8+ （云平台看板）

### 构建

```bash
# 克隆仓库
git clone <repo-url>
cd VisionTool

# 配置构建
cmake -B build/Debug -DCMAKE_PREFIX_PATH="path/to/Qt/6.x.x/msvc2022_64"

# 编译
cmake --build build/Debug

# 运行
./build/Debug/bin/VisionTool.exe
```

### 云平台启动

```bash
cd cloud_dashboard
pip install -r requirements.txt
python app.py
```

浏览器访问 `http://localhost:5000` 即可查看云平台看板。

---

## 项目结构

```
VisionTool/
├── 3rdparty/                  # 第三方库
│   ├── opencv/                # OpenCV 4.x
│   ├── paho-mqtt/             # Paho MQTT C++
│   ├── spdlog/                # spdlog 日志库
│   └── zxing/                 # ZXing-CPP 条码识别
├── include/                   # 头文件
│   ├── algorithm/             # 算法层：图像处理、DNN、条码、模板匹配
│   ├── config/                # 配置层：Pipeline、ROI、检测、显示等配置
│   ├── controllers/           # 控制器层：批量检测、ROI交互、配置管理
│   ├── core/                  # 核心层：Pipeline、ROI管理、方案管理、MQTT
│   ├── data/                  # 数据结构：区域特征、条码结果、检测报告
│   ├── ui/                    # UI组件：主窗口、图像视图、日志、文件管理
│   └── widgets/               # Widget组件：各功能Tab、ROI列表、图片列表
├── src/                       # 源文件
│   ├── algorithm/
│   ├── config/
│   ├── controllers/
│   ├── core/
│   ├── ui/
│   ├── widgets/
│   └── main.cpp               # 入口
├── ui/                        # Qt Designer UI 文件
│   └── tabs/                  # 各功能Tab的UI布局
├── resources/                 # 资源文件
│   ├── icons/                 # 图标
│   ├── models/                # 深度学习模型 (yolov8n.onnx)
│   ├── template/              # 模板图像
│   ├── style.qss              # 全局样式表
│   └── res.qrc                # Qt 资源文件
├── cloud_dashboard/           # 云平台看板 (Python Flask)
│   ├── app.py                 # Flask 后端 + MQTT 订阅
│   ├── templates/index.html   # 前端页面
│   ├── requirements.txt       # Python 依赖
│   └── dashboard.db           # SQLite 数据库
├── logs/                      # 运行日志
├── bin/                       # 构建输出目录
├── CMakeLists.txt             # CMake 构建配置
└── app_config.json            # 应用默认配置
```

---

## 边云协同

### 通信架构

```
┌─────────────────┐          MQTT Broker          ┌──────────────────┐
│  EdgeVision     │◄─────── (MQTT) ──────────────►│  Cloud Dashboard │
│  (C++ Desktop)  │                                │  (Python Flask)  │
│                 │  visiontool/results            │                  │
│  ● 结果上报     │──────────────────────────────►│  ● 实时显示      │
│  ● 心跳保活     │  visiontool/heartbeat          │  ● 统计分析      │
│  ● 指令执行     │──────────────────────────────►│  ● 数据持久化    │
│                 │  visiontool/commands           │  ● 远程指令      │
│                 │◄───────────────────────────────│                  │
└─────────────────┘                                └──────────────────┘
```

### 数据格式

检测结果以标准化 JSON 格式上报：

```json
{
  "reportId": "a1b2c3d4e5f6",
  "deviceId": "VisionTool-001",
  "roiName": "PCB-焊点检测",
  "timestamp": 1713945600000,
  "result": {
    "passed": true,
    "totalRegionCount": 5,
    "originalRegionCount": 8
  },
  "regions": [
    {
      "index": 1,
      "area": 256.5,
      "circularity": 0.85,
      "centerX": 320.0,
      "centerY": 240.0
    }
  ],
  "barcodes": [
    {
      "type": "QRCode",
      "data": "SN-2024-A001",
      "quality": 1.0
    }
  ]
}
```

---

## 开发指南

### 设计原则

- **解耦**: 通过 `ISignalConnectable` 接口解耦 Tab Widget 与核心模块；通过 `Controllers` 层分离业务逻辑与 UI
- **单职**: 每个类职责单一，MainWindow 通过组合多个 Manager/Controller 来编排功能
- **可扩展**: 新增检测类型只需添加 `DetectionType` 枚举项和对应配置结构体；新增算法只需添加 `AlgorithmStep`
- **线程安全**: PipelineManager 使用互斥锁 + 原子标志确保配置读写安全；后台 Pipeline 执行与 UI 线程通过值拷贝交换数据

### 扩展 Pipeline 步骤

1. 在 `pipeline_steps.h` 中实现 `IPipelineStep` 子类
2. 在 `pipeline_steps.cpp` 中实现 `run()` 方法
3. 在 `PipelineManager::initPipeline()` 中注册步骤

### 新增检测类型

1. 在 `detection_type.h` 中添加 `DetectionType` 枚举值
2. 在 `detection_config_types.h` 中添加配置结构体（实现 `toJson` / `fromJson`）
3. 在 `DetectionItem` 构造函数中初始化默认配置
4. 创建对应的 Tab Widget，实现 `ISignalConnectable` 接口

---

## 许可证

本项目基于 MIT 许可证开源。详见 [LICENSE](LICENSE) 文件。
