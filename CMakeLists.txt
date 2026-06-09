cmake_minimum_required(VERSION 3.16)

project(VisionTool VERSION 2.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)


# 设置构建输出目录 (仅在未指定时设置默认值)
if(NOT CMAKE_BINARY_DIR)
    set(CMAKE_BINARY_DIR "${CMAKE_SOURCE_DIR}/build/Debug")
endif()

# 设置输出目录
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# 告诉 CMake 的 AUTOUIC 在哪些目录中查找 .ui 文件
list(APPEND CMAKE_AUTOUIC_SEARCH_PATHS 
    ${CMAKE_CURRENT_SOURCE_DIR}/ui 
    ${CMAKE_CURRENT_SOURCE_DIR}/ui/tabs
)

# ============================================================
# 查找 Qt6 库
# ============================================================
find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets)

# ============================================================
# 包含路径配置
# ============================================================
set(INCLUDE_DIRS
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/include/core
    ${CMAKE_CURRENT_SOURCE_DIR}/include/algorithm
    ${CMAKE_CURRENT_SOURCE_DIR}/include/config
    ${CMAKE_CURRENT_SOURCE_DIR}/include/data
    ${CMAKE_CURRENT_SOURCE_DIR}/include/ui
    ${CMAKE_CURRENT_SOURCE_DIR}/include/widgets
    ${CMAKE_CURRENT_SOURCE_DIR}/include/controllers
    ${CMAKE_CURRENT_SOURCE_DIR}/include/utils
)

# ============================================================
# 源文件收集（显式列出，新增文件时需手动添加）
# ============================================================
set(HEADERS
    # core
    include/core/logger.h
    include/core/mqtt_client.h
    include/core/mqtt_manager.h
    include/core/pipeline.h
    include/core/pipeline_manager.h
    include/core/pipeline_request.h
    include/core/pipeline_result.h
    include/core/pipeline_scheduler.h
    include/core/pipeline_steps.h
    include/core/profile_manager.h
    include/core/roi_manager.h
    include/core/video_manager.h
    include/core/barcode_step.h
    include/core/i_pipeline_access.h
    # algorithm
    include/algorithm/dnn_inference.h
    include/algorithm/detection_evaluator.h
    include/algorithm/image_processor.h
    include/algorithm/image_utils.h
    include/algorithm/match_strategy.h
    include/algorithm/opencv_algorithm.h
    include/algorithm/ort_inference.h
    include/algorithm/zxing_barcode_reader.h
    include/algorithm/display_renderer.h
    # config
    include/config/algorithm_step.h
    include/config/config_manager.h
    include/config/constants.h
    include/config/detection_config_types.h
    include/config/detection_item.h
    include/config/detection_type.h
    include/config/display_config.h
    include/config/json_serializable.h
    include/config/mqtt_config.h
    include/config/pipeline_config.h
    include/config/roi_config.h
    include/config/shape_filter_types.h
    include/config/tab_config.h
    # config (独立配置头文件)
    include/config/enhance_config.h
    include/config/color_filter_config.h
    include/config/image_filter_config.h
    include/config/ocr_config.h
    include/config/object_detection_config.h
    include/config/line_detect_config.h
    include/config/blob_judge_config.h
    include/config/barcode_config.h
    # data
    include/data/barcode_result.h
    include/data/detection_result_report.h
    include/data/inspection_profile.h
    include/data/region_feature.h
    # ui
    include/ui/cloud_dashboard_manager.h
    include/ui/display_mode_manager.h
    include/ui/file_manager.h
    include/ui/image_view.h
    include/ui/log_page.h
    include/ui/mainwindow.h
    include/ui/slider_spinbox_binder.h
    include/ui/system_monitor.h
    include/ui/toast_notification.h
    # widgets
    include/widgets/barcode_tab_widget.h
    include/widgets/batch_detection_widget.h
    include/widgets/batch_match_dialog.h
    include/widgets/enhance_tab_widget.h
    include/widgets/extract_tab_widget.h
    include/widgets/filter_tab_widget.h
    include/widgets/i_tab_interfaces.h
    include/widgets/image_list_manager.h
    include/widgets/image_tab_widget.h
    include/widgets/judge_tab_widget.h
    include/widgets/line_tab_widget.h
    include/widgets/object_detection_tab_widget.h
    include/widgets/process_tab_widget.h
    include/widgets/profile_tab_widget.h
    include/widgets/roi_detection_config_dialog.h
    include/widgets/tab_manager.h
    include/widgets/tab_registry.h
    include/widgets/template_tab_widget.h
    include/widgets/step_config_widget.h
    include/widgets/video_source_widget.h
    include/widgets/video_tab_widget.h
    include/widgets/image_filter_tab_widget.h
    include/widgets/ocr_tab_widget.h
    include/core/ocr_step.h
    include/data/ocr_region.h
    # controllers
    include/controllers/auto_detection_controller.h
    include/controllers/config_controller.h
    include/controllers/detection_ui_controller.h
    include/controllers/profile_controller.h
    include/controllers/pipeline_result_handler.h
    include/controllers/roi_ui_controller.h
    include/controllers/roi_detection_config_controller.h
    # utils
    include/utils/path_utils.h
    include/utils/benchmark.h
    include/utils/model_warmup.h
)

set(SOURCES
    src/main.cpp
    # core
    src/core/logger.cpp
    src/core/mqtt_client.cpp
    src/core/mqtt_manager.cpp
    src/core/pipeline.cpp
    src/core/pipeline_manager.cpp
    src/core/pipeline_scheduler.cpp
    src/core/pipeline_steps.cpp
    src/core/profile_manager.cpp
    src/core/roi_manager.cpp
    src/core/video_manager.cpp
    src/core/barcode_step.cpp
    src/core/pipeline_image_filter_steps.cpp
    src/core/ocr_step.cpp
    # algorithm
    src/algorithm/dnn_inference.cpp
    src/algorithm/detection_evaluator.cpp
    src/algorithm/image_processor.cpp
    src/algorithm/image_utils.cpp
    src/algorithm/match_strategy.cpp
    src/algorithm/opencv_algorithm.cpp
    src/algorithm/ort_inference.cpp
    src/algorithm/zxing_barcode_reader.cpp
    src/algorithm/display_renderer.cpp
    # data
    src/data/inspection_profile.cpp
    # config
    src/config/config_manager.cpp
    src/config/pipeline_config.cpp
    src/config/roi_config.cpp
    src/config/detection_item.cpp
    src/config/detection_config_types.cpp
    # ui
    src/ui/cloud_dashboard_manager.cpp
    src/ui/display_mode_manager.cpp
    src/ui/file_manager.cpp
    src/ui/image_view.cpp
    src/ui/log_page.cpp
    src/ui/mainwindow.cpp
    src/ui/system_monitor.cpp
    src/ui/toast_notification.cpp
    # widgets
    src/widgets/barcode_tab_widget.cpp
    src/widgets/batch_detection_widget.cpp
    src/widgets/batch_match_dialog.cpp
    src/widgets/enhance_tab_widget.cpp
    src/widgets/extract_tab_widget.cpp
    src/widgets/filter_tab_widget.cpp
    src/widgets/image_list_manager.cpp
    src/widgets/image_tab_widget.cpp
    src/widgets/judge_tab_widget.cpp
    src/widgets/line_tab_widget.cpp
    src/widgets/object_detection_tab_widget.cpp
    src/widgets/process_tab_widget.cpp
    src/widgets/profile_tab_widget.cpp
    src/widgets/roi_detection_config_dialog.cpp
    src/widgets/tab_manager.cpp
    src/widgets/tab_registry.cpp
    src/widgets/template_tab_widget.cpp
    src/widgets/step_config_widget.cpp
    src/widgets/video_source_widget.cpp
    src/widgets/video_tab_widget.cpp
    src/widgets/image_filter_tab_widget.cpp
    src/widgets/ocr_tab_widget.cpp
    # controllers
    src/controllers/auto_detection_controller.cpp
    src/controllers/config_controller.cpp
    src/controllers/detection_ui_controller.cpp
    src/controllers/pipeline_result_handler.cpp
    src/controllers/profile_controller.cpp
    src/controllers/roi_ui_controller.cpp
    src/controllers/roi_detection_config_controller.cpp
    # utils
    src/utils/model_warmup.cpp
)

set(FORMS
    ui/mainwindow.ui
    ui/home_page.ui
    ui/log_page.ui
)

# 资源文件 (RESOURCES)
set(RESOURCES
    resources/res.qrc
)

# ============================================================
# 创建可执行文件目标
# ============================================================
add_executable(VisionTool
    ${HEADERS}
    ${SOURCES}
    ${FORMS}
    ${RESOURCES}
)

# 设置包含路径
target_include_directories(VisionTool PRIVATE ${INCLUDE_DIRS})

# 定义项目根目录宏，供代码使用
target_compile_definitions(VisionTool PRIVATE PROJECT_ROOT_DIR="${CMAKE_CURRENT_SOURCE_DIR}")

# 链接 Qt 库
target_link_libraries(VisionTool PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
)

# Windows 系统库 (PDH 性能监控)
if(WIN32)
    target_link_libraries(VisionTool PRIVATE pdh)
endif()

# ============================================================
# 第三方库配置 (OpenCV)
# ============================================================
set(OPENCV_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/opencv")
set(OPENCV_DEBUG_LIB_DIR "${OPENCV_ROOT}/lib")
set(OPENCV_RELEASE_LIB_DIR "${OPENCV_ROOT}/release/lib")
set(OPENCV_RELEASE_BIN_DIR "${OPENCV_ROOT}/release/bin")

# OpenCV 包含路径
target_include_directories(VisionTool PRIVATE
    "${OPENCV_ROOT}/include/opencv4"
    "${OPENCV_ROOT}/include/opencv4/opencv2/ximgproc"
)

# OpenCV 模块列表
set(OPENCV_MODULES
    core imgcodecs highgui imgproc videoio dnn photo
    ximgproc line_descriptor features2d calib3d flann video
)

# Debug 链 opencv_xxx4d.lib，Release 链 opencv_xxx4.lib
foreach(MODULE ${OPENCV_MODULES})
    target_link_libraries(VisionTool PRIVATE
        debug "${OPENCV_DEBUG_LIB_DIR}/opencv_${MODULE}4d.lib"
        optimized "${OPENCV_RELEASE_LIB_DIR}/opencv_${MODULE}4.lib"
    )
endforeach()

# 收集 Release DLL 路径
set(OPENCV_RELEASE_DLLS)
foreach(MODULE ${OPENCV_MODULES})
    list(APPEND OPENCV_RELEASE_DLLS "${OPENCV_RELEASE_BIN_DIR}/opencv_${MODULE}4.dll")
endforeach()

# 拷贝 Release DLL 到输出目录
add_custom_command(TARGET VisionTool POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${OPENCV_RELEASE_DLLS}
        $<TARGET_FILE_DIR:VisionTool>
    COMMENT "Copying OpenCV Release DLLs..."
)

# OpenCV 传递依赖 DLL (仅 Release 拷贝，Debug 从 PATH 加载)
if(CMAKE_BUILD_TYPE STREQUAL "Release")
    add_custom_command(TARGET VisionTool POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${OPENCV_RELEASE_BIN_DIR}/zlib1.dll"
            "${OPENCV_RELEASE_BIN_DIR}/jpeg62.dll"
            "${OPENCV_RELEASE_BIN_DIR}/libwebp.dll"
            "${OPENCV_RELEASE_BIN_DIR}/libwebpdecoder.dll"
            "${OPENCV_RELEASE_BIN_DIR}/libwebpdemux.dll"
            "${OPENCV_RELEASE_BIN_DIR}/libwebpmux.dll"
            "${OPENCV_RELEASE_BIN_DIR}/libpng16.dll"
            "${OPENCV_RELEASE_BIN_DIR}/tiff.dll"
            "${OPENCV_RELEASE_BIN_DIR}/libsharpyuv.dll"
            "${OPENCV_RELEASE_BIN_DIR}/liblzma.dll"
            "${OPENCV_RELEASE_BIN_DIR}/libprotobuf.dll"
            "${OPENCV_RELEASE_BIN_DIR}/abseil_dll.dll"
            "${OPENCV_RELEASE_BIN_DIR}/lz4.dll"
            "${OPENCV_RELEASE_BIN_DIR}/zstd.dll"
            "${OPENCV_RELEASE_BIN_DIR}/bz2.dll"
            $<TARGET_FILE_DIR:VisionTool>
        COMMENT "Copying OpenCV vcpkg dependency DLLs..."
    )
endif()


# ============================================================
# 第三方库配置 (ONNX Runtime GPU)
# ============================================================
set(ONNXRT_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/onnxruntime")

target_include_directories(VisionTool PRIVATE
    "${ONNXRT_ROOT}/include"
)

target_link_libraries(VisionTool PRIVATE
    debug "${ONNXRT_ROOT}/lib/onnxruntime.lib"
    optimized "${ONNXRT_ROOT}/lib/onnxruntime.lib"
)

# 拷贝 ONNX Runtime DLL 到输出目录
add_custom_command(TARGET VisionTool POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${ONNXRT_ROOT}/lib/onnxruntime.dll"
        $<TARGET_FILE_DIR:VisionTool>
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${ONNXRT_ROOT}/lib/onnxruntime_providers_cuda.dll"
        $<TARGET_FILE_DIR:VisionTool>
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${ONNXRT_ROOT}/lib/onnxruntime_providers_shared.dll"
        $<TARGET_FILE_DIR:VisionTool>
    COMMENT "Copying ONNX Runtime DLLs..."
)

# ============================================================
# 第三方库配置 (spdlog, header-only)
# ============================================================
target_include_directories(VisionTool PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/spdlog/include"
)
target_compile_definitions(VisionTool PRIVATE SPDLOG_USE_STD_FORMAT)

# =========================
# ZXing
# =========================

set(ZXING_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/zxing")

target_include_directories(VisionTool PRIVATE
    "${ZXING_ROOT}/include"
)

target_link_libraries(VisionTool PRIVATE
    debug "${ZXING_ROOT}/lib/ZXingd.lib"
    optimized "${ZXING_ROOT}/release/lib/ZXing.lib"
)


# ============================================================
# 第三方库配置 (Tesseract OCR)
# ============================================================
set(TESSERACT_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/tesseract")

target_include_directories(VisionTool PRIVATE
    "${TESSERACT_ROOT}/include"
)

target_link_libraries(VisionTool PRIVATE
    debug "${TESSERACT_ROOT}/lib/tesseract55d.lib"
    optimized "${TESSERACT_ROOT}/lib/tesseract55.lib"
)

# 拷贝 Tesseract DLL 到输出目录
add_custom_command(TARGET VisionTool POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${TESSERACT_ROOT}/bin/tesseract55d.dll"
        $<TARGET_FILE_DIR:VisionTool>
    COMMENT "Copying Tesseract DLL..."
)


# ============================================================
# 第三方库配置 (Paho MQTT)
# ============================================================
set(PAHO_MQTT_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/paho-mqtt")

target_include_directories(VisionTool PRIVATE
    "${PAHO_MQTT_ROOT}/include"
)

# 必须定义导入宏，否则 MSVC 无法解析 DLL 中的数据符号（如 mqtt::message::EMPTY_STR）
target_compile_definitions(VisionTool PRIVATE
    PAHO_MQTTPP_IMPORTS
    PAHO_MQTT_IMPORTS
)

# Paho MQTT C++ wrapper + C core
target_link_libraries(VisionTool PRIVATE
    debug "${PAHO_MQTT_ROOT}/lib/paho-mqttpp3d.lib"
    optimized "${PAHO_MQTT_ROOT}/release/lib/paho-mqttpp3.lib"
    debug "${PAHO_MQTT_ROOT}/lib/paho-mqtt3asd.lib"
    optimized "${PAHO_MQTT_ROOT}/release/lib/paho-mqtt3as.lib"
)

# 拷贝 Paho MQTT + OpenSSL DLL (仅 Release，Debug 通过 PATH 加载)
if(CMAKE_BUILD_TYPE STREQUAL "Release")
    add_custom_command(TARGET VisionTool POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${PAHO_MQTT_ROOT}/release/bin/paho-mqttpp3.dll"
            $<TARGET_FILE_DIR:VisionTool>
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${PAHO_MQTT_ROOT}/release/bin/paho-mqtt3as.dll"
            $<TARGET_FILE_DIR:VisionTool>
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${PAHO_MQTT_ROOT}/release/bin/libssl-3-x64.dll"
            $<TARGET_FILE_DIR:VisionTool>
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${PAHO_MQTT_ROOT}/release/bin/libcrypto-3-x64.dll"
            $<TARGET_FILE_DIR:VisionTool>
        COMMENT "Copying Paho MQTT + OpenSSL DLLs..."
    )
endif()

# ============================================================
# 编译器特定选项 (MSVC)
# ============================================================
if(MSVC)
    # 添加 MSVC 编译选项
    target_compile_options(VisionTool PRIVATE
        /W3   # 警告级别 3
        /EHsc # 启用 C++ 异常处理
        /Zi   # 调试信息格式（Debug/RelWithDebInfo 下有用）
        $<$<CONFIG:Debug>:/MDd>         # Debug: 多线程调试 DLL 运行时
        $<$<CONFIG:Release>:/MD>        # Release: 多线程 DLL 运行时
        $<$<CONFIG:RelWithDebInfo>:/MD> # RelWithDebInfo: 多线程 DLL 运行时
    )
    # 添加链接器选项（仅 Debug 生成完整调试信息）
    target_link_options(VisionTool PRIVATE
        $<$<CONFIG:Debug>:/DEBUG>
    )
endif()


# ============================================================
# 编译输出配置
# ============================================================
set_target_properties(VisionTool PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
)
