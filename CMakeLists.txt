cmake_minimum_required(VERSION 3.16)

project(VisionTool LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)


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
# 编译器配置 - 使用 Ninja 生成器
# ============================================================
set(CMAKE_MAKE_PROGRAM "F:/IDE/Qt/Tools/Ninja/ninja.exe" CACHE PATH "Path to Ninja" FORCE)

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
)

# ============================================================
# 源文件收集
# ============================================================
file(GLOB HEADERS
    include/*.h
    include/core/*.h
    include/algorithm/*.h
    include/config/*.h
    include/data/*.h
    include/ui/*.h
    include/widgets/*.h
    include/controllers/*.h
)

file(GLOB SOURCES
    src/*.cpp
    src/core/*.cpp
    src/algorithm/*.cpp
    src/config/*.cpp
    src/ui/*.cpp
    src/widgets/*.cpp
    src/controllers/*.cpp
)

file(GLOB FORMS
    ui/*.ui
    ui/tabs/*.ui
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
# 第三方库配置 (spdlog)
# ============================================================
set(SPDLOG_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/spdlog")

# spdlog 包含路径
target_include_directories(VisionTool PRIVATE
    "${SPDLOG_ROOT}/include"
)

# spdlog 库文件路径
set(SPDLOG_LIB_DIR "${SPDLOG_ROOT}/lib")

# 查找并链接 spdlog 库
find_library(SPDLOG_LIB
    NAMES spdlogd spdlog
    PATHS "${SPDLOG_LIB_DIR}"
    NO_DEFAULT_PATH
)

if(SPDLOG_LIB)
    target_link_libraries(VisionTool PRIVATE ${SPDLOG_LIB})
    message(STATUS "Found spdlog library: ${SPDLOG_LIB}")
else()
    message(WARNING "spdlog library not found at ${SPDLOG_LIB_DIR}")
endif()

# 查找并链接 fmt 库
find_library(FMT_LIB
    NAMES fmtd fmt
    PATHS "${SPDLOG_LIB_DIR}"
    NO_DEFAULT_PATH
)

if(FMT_LIB)
    target_link_libraries(VisionTool PRIVATE ${FMT_LIB})
    message(STATUS "Found fmt library: ${FMT_LIB}")
else()
    message(WARNING "fmt library not found at ${SPDLOG_LIB_DIR}")
endif()

# 添加 SPDLOG_FMT_EXTERNAL 宏定义
target_compile_definitions(VisionTool PRIVATE SPDLOG_FMT_EXTERNAL)

# =========================
# ZXing
# =========================

set(ZXING_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/zxing")

target_include_directories(VisionTool PRIVATE
    "${ZXING_ROOT}/include"
)

target_link_libraries(VisionTool PRIVATE
    "${ZXING_ROOT}/lib/ZXing.lib"
)


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
