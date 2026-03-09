QT       += core gui
QMAKE_PROJECT_DEPTH = 0
greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

CONFIG += c++17

INCLUDEPATH += $$PWD/include
INCLUDEPATH += $$PWD/controllers

# DEFINES += PROJECT_DIR=\\\"$$PWD\\\"

# ===== OpenCV 配置 (使用本地 3rdparty) =====
OPENCV_DIR = $$PWD/3rdparty/opencv
INCLUDEPATH += $$OPENCV_DIR/include/opencv4

LIBS += -L$$OPENCV_DIR/lib \
        -lopencv_core4d \
        -lopencv_imgcodecs4d \
        -lopencv_highgui4d \
        -lopencv_imgproc4d \
        -lopencv_videoio4d \
        -lopencv_dnn4d \
        -lopencv_photo4d

# ===== HALCON 配置 (使用本地 3rdparty) =====
HALCON_DIR = $$PWD/3rdparty/halcon
INCLUDEPATH += $$HALCON_DIR/include \
               $$HALCON_DIR/include/halconcpp

LIBS += -L$$HALCON_DIR/lib \
        -lhalconcppxl \
        -lhalconxl
        
# ===== 头文件 =====
HEADERS += \
    include/halcon_algorithm.h \
    include/image_processor.h \
    include/image_utils.h \
    include/image_view.h \
    include/logger.h \
    include/mainwindow.h \
    include/pipeline.h \
    include/pipeline_manager.h \
    include/pipeline_steps.h \
    include/shape_filter_types.h \
    include/system_monitor.h \
    include/match_strategy.h \
    include/file_manager.h \
    include/roi_manager.h \
    include/template_controller.h \
    controllers/image_tab_controller.h \
    controllers/enhancement_tab_controller.h \
    controllers/filter_tab_controller.h \
    controllers/algorithm_tab_controller.h \
    controllers/extract_tab_controller.h

# ===== 源文件 =====
SOURCES += \
    src/match_strategy.cpp \
    src/halcon_algorithm.cpp \
    src/image_processor.cpp \
    src/image_utils.cpp \
    src/image_view.cpp \
    src/logger.cpp \
    src/main.cpp \
    src/mainwindow.cpp \
    src/pipeline.cpp \
    src/pipeline_manager.cpp \
    src/system_monitor.cpp \
    src/file_manager.cpp \
    src/roi_manager.cpp \
    src/template_controller.cpp \
    controllers/image_tab_controller.cpp \
    controllers/enhancement_tab_controller.cpp \
    controllers/filter_tab_controller.cpp \
    controllers/algorithm_tab_controller.cpp \
    controllers/extract_tab_controller.cpp

# ===== UI 文件 =====
FORMS += \
    ui/mainwindow.ui

# ===== 资源文件 =====
RESOURCES += \
    resources/res.qrc
