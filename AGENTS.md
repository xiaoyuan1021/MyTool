# AGENTS.md — EdgeVision

## What This Is

C++20 / Qt6 desktop vision inspection app + Python Flask cloud dashboard.
Windows-only (MSVC 2022). Two independent build systems.

## Build: Desktop App (C++)

Generator: **Ninja**. Requires MSVC 2022 + Qt6. One-time ZXing rebuild needed if `3rdparty/zxing/lib/` is empty.

```powershell
# First-time: build ZXing
mkdir 3rdparty\zxing\lib  # if missing
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"
cmake -S 3rdparty/zxing/zxing-cpp -B build_zxing_debug -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build_zxing_debug --config Debug
copy build_zxing_debug\core\lib\ZXingd.lib 3rdparty\zxing\lib\ZXingd.lib
```

```powershell
# Regular build (from repo root, uses build/Debug as working dir)
mkdir build\Debug  # if missing
cd build\Debug
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"
cmake -G Ninja -DCMAKE_BUILD_TYPE=Debug ../..
ninja
```

Output: `build/Debug/bin/VisionTool.exe`

- CMake source files are **explicitly listed** in `CMakeLists.txt`. New `.cpp`/`.h` must be added to `SOURCES`/`HEADERS` lists.
- All third-party libs are vendored in `3rdparty/` (OpenCV, ONNX Runtime, Paho MQTT, spdlog, ZXing, Tesseract). Do not use system-installed versions.
- Ninja single-config: must pass `-DCMAKE_BUILD_TYPE=Debug` at configure time (no `--config` flag).
- Debug builds load DLLs from PATH; Release builds copy them to output dir. Release config adds `C:\vcpkg\installed\x64-windows\bin` to PATH.
- Compile defines required: `SPDLOG_USE_STD_FORMAT`, `PAHO_MQTTPP_IMPORTS`, `PAHO_MQTT_IMPORTS`, `PROJECT_ROOT_DIR`.
- Qt AUTOUIC search paths: `ui/` and `ui/tabs/`. New `.ui` files go there (already registered).

## Build: Cloud Dashboard (Python)

```bash
cd cloud_dashboard
pip install -r requirements.txt
python app.py
# or: docker compose up --build  (includes EMQX broker)
```

Flask on port 5000, SQLite at `cloud_dashboard/dashboard.db`.
Docker: EMQX 5.8 broker + Flask dashboard via `docker-compose.yml`.

## Architecture

Six-layer C++ architecture, dependency flows top-down:

```
UI (MainWindow, ImageView, Toast)
  → Widgets (13 tab widgets: Image, Video, Enhance, Filter, Process, Extract, Judge, Line, Template, Barcode, ObjectDetection, Batch, Profile, Ocr)
    → Controllers (AutoDetection, DetectionUi, RoiUi, Profile, Config, PipelineResultHandler)
      → Core (Pipeline, PipelineManager, RoiManager, ProfileManager, VideoManager, MqttManager) + Algorithm (ImageProcessor, OpenCVAlgorithm, OrtInference, ZXingBarcodeReader, TesseractOcrEngine)
        → Config/Data (PipelineConfig, DetectionItem, RoiConfig, RegionFeature, BarcodeResult, OcrConfig)
```

Entry point: `src/main.cpp` → `MainWindow`.

## Adding a Pipeline Step

1. Subclass `IPipelineStep` in `include/core/pipeline_steps.h`
2. Implement `run()` in `src/core/pipeline_steps.cpp`
3. Register in `PipelineManager::initPipeline()`

## Adding a Detection Type

1. Add enum in `include/config/detection_type.h`
2. Add config struct in `include/config/detection_config_types.h`
3. Initialize defaults in `DetectionItem` constructor
4. Create tab widget implementing `ISignalConnectable` interface

## MQTT Topics (Edge ↔ Cloud)

| Topic | Direction | Purpose |
|-------|-----------|---------|
| `visiontool/results` | Edge → Cloud | Detection results |
| `visiontool/heartbeat` | Edge → Cloud | Device heartbeat |
| `visiontool/commands` | Cloud → Edge | Remote commands |

Config: `app_config.json` (edge), `cloud_dashboard/config.py` (cloud).

## Key Conventions

- All tab widgets implement `ISignalConnectable` for decoupled signal wiring.
- `PipelineManager` runs async with mutex + atomic flags + value-swap data passing.
- ROI coordinates are normalized (0–1) for resolution independence.
- Pipeline steps are controlled by `stepEnabled` array and `stepOrder` in config.
- Resources: `resources/res.qrc`, style in `resources/style.qss`, ONNX models in `resources/models/`, tessdata in `resources/tessdata/`.
