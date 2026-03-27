cmake_minimum_required(VERSION 3.16)

project(VisionTool LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

# 告诉 CMake 的 AUTOUIC 在哪些目录中查找 .ui 文件
list(APPEND CMAKE_AUTOUIC_SEARCH_PATHS 
    ${CMAKE_CURRENT_SOURCE_DIR}/ui 
    ${CMAKE_CURRENT_SOURCE_DIR}/ui/tabs
)

# ============================================================
# 编译器配置 - 使用 Ninja 生成器
# ============================================================
# set(CMAKE_MAKE_PROGRAM "F:/IDE/Qt/Tools/Ninja/ninja.exe" CACHE PATH "Path to Ninja" FORCE)

# ============================================================
# 查找 Qt6 库
# ============================================================
find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets)

# ============================================================
# 包含路径配置
# ============================================================
set(INCLUDE_DIRS
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/include/widgets
)

# ============================================================
# 源文件收集
# ============================================================

# 头文件 (HEADERS)
set(HEADERS
    include/halcon_algorithm.h
    include/image_processor.h
    include/image_utils.h
    include/image_view.h
    include/logger.h
    include/mainwindow.h
    include/pipeline.h
    include/pipeline_manager.h
    include/pipeline_steps.h
    include/barcode_step.h
    include/shape_filter_types.h
    include/system_monitor.h
    include/match_strategy.h
    include/file_manager.h
    include/roi_manager.h
    include/log_page.h
    include/widgets/image_tab_widget.h
    include/widgets/enhance_tab_widget.h
    include/widgets/filter_tab_widget.h
    include/widgets/template_tab_widget.h
    include/widgets/line_tab_widget.h
    include/widgets/extract_tab_widget.h
    include/widgets/process_tab_widget.h
    include/widgets/judge_tab_widget.h
    include/widgets/barcode_tab_widget.h
    include/config_manager.h
)

# 源文件 (SOURCES)
set(SOURCES
    src/match_strategy.cpp
    src/halcon_algorithm.cpp
    src/image_processor.cpp
    src/image_utils.cpp
    src/image_view.cpp
    src/logger.cpp
    src/main.cpp
    src/mainwindow.cpp
    src/pipeline.cpp
    src/pipeline_manager.cpp
    src/system_monitor.cpp
    src/file_manager.cpp
    src/roi_manager.cpp
    src/pipeline_steps.cpp
    src/barcode_step.cpp
    src/log_page.cpp
    src/widgets/image_tab_widget.cpp
    src/widgets/enhance_tab_widget.cpp
    src/widgets/filter_tab_widget.cpp
    src/widgets/template_tab_widget.cpp
    src/widgets/line_tab_widget.cpp
    src/widgets/extract_tab_widget.cpp
    src/widgets/process_tab_widget.cpp
    src/widgets/judge_tab_widget.cpp
    src/widgets/barcode_tab_widget.cpp
    src/config_manager.cpp
)

# UI 文件 (FORMS)
set(FORMS
    ui/mainwindow.ui
    ui/log_page.ui
    ui/tabs/image_tab.ui
    ui/tabs/enhance_tab.ui
    ui/tabs/filter_tab.ui
    ui/tabs/template_tab.ui
    ui/tabs/line_tab.ui
    ui/tabs/extract_tab.ui
    ui/tabs/process_tab.ui
    ui/tabs/judge_tab.ui
    ui/tabs/barcode_tab.ui
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

# 链接 Qt 库
target_link_libraries(VisionTool PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
)

# ============================================================
# 第三方库配置 (OpenCV)
# ============================================================
set(OPENCV_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/opencv")

# OpenCV 包含路径
target_include_directories(VisionTool PRIVATE
    "${OPENCV_ROOT}/include/opencv4"
    "${OPENCV_ROOT}/include/opencv4/opencv2/ximgproc"
)

# OpenCV 库文件路径
set(OPENCV_LIB_DIR "${OPENCV_ROOT}/lib")

# 根据构建类型选择 OpenCV 库后缀
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(OPENCV_SUFFIX "")
else()
    set(OPENCV_SUFFIX "")
endif()

# OpenCV 库列表
set(OPENCV_LIBS
    opencv_core4d
    opencv_imgcodecs4d
    opencv_highgui4d
    opencv_imgproc4d
    opencv_videoio4d
    opencv_dnn4d
    opencv_photo4d
    opencv_ximgproc4d
    opencv_line_descriptor4d
    opencv_features2d4d
    opencv_calib3d4d
    opencv_flann4d
    opencv_video4d
)

foreach(OPENCV_LIB ${OPENCV_LIBS})
    set(OPENCV_LIB_NAME "${OPENCV_LIB}${OPENCV_SUFFIX}")
    find_library(OPENCV_${OPENCV_LIB}_PATH
        NAMES ${OPENCV_LIB_NAME}
        PATHS "${OPENCV_LIB_DIR}"
        NO_DEFAULT_PATH
    )
    if(OPENCV_${OPENCV_LIB}_PATH)
        target_link_libraries(VisionTool PRIVATE ${OPENCV_${OPENCV_LIB}_PATH})
        message(STATUS "Found OpenCV library: ${OPENCV_LIB_NAME}")
    else()
        message(WARNING "OpenCV library not found: ${OPENCV_LIB_NAME}")
    endif()
endforeach()

# ============================================================
# 第三方库配置 (Halcon)
# ============================================================
set(HALCON_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/halcon")

# Halcon 包含路径
target_include_directories(VisionTool PRIVATE
    "${HALCON_ROOT}/include"
    "${HALCON_ROOT}/include/halconcpp"
)

# Halcon 库文件路径
set(HALCON_LIB_DIR "${HALCON_ROOT}/lib")

# 查找并链接 Halcon 库
find_library(HALCON_CPP_LIB
    NAMES halconcppxl halconcpp
    PATHS "${HALCON_LIB_DIR}"
    NO_DEFAULT_PATH
)

find_library(HALCON_LIB
    NAMES halconxl halcon
    PATHS "${HALCON_LIB_DIR}"
    NO_DEFAULT_PATH
)

if(HALCON_CPP_LIB AND HALCON_LIB)
    target_link_libraries(VisionTool PRIVATE ${HALCON_CPP_LIB} ${HALCON_LIB})
    message(STATUS "Found Halcon libraries")
else()
    message(WARNING "Halcon libraries not found at ${HALCON_LIB_DIR}")
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
