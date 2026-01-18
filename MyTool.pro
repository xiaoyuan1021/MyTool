QT       += core gui
QMAKE_PROJECT_DEPTH = 0
greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

CONFIG += c++17

# You can make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

INCLUDEPATH += C:/vcpkg/installed/x64-windows/include/opencv4

LIBS += -LC:/vcpkg/installed/x64-windows/debug/lib \
        -lopencv_core4d \
        -lopencv_imgcodecs4d \
        -lopencv_highgui4d \
        -lopencv_imgproc4d \
        -lopencv_videoio4d \
        -lopencv_dnn4d \
        -lopencv_photo4d

# ===== HALCON 配置 =====
# HALCON 根目录
HALCON_DIR = F:/Halcon

# HALCON 头文件路径
INCLUDEPATH += $$HALCON_DIR/include
INCLUDEPATH += $$HALCON_DIR/include/halconcpp

# HALCON 库文件路径及链接库
LIBS += -L$$HALCON_DIR/lib/x64-win64 \
        -lhalconcppxl \
        -lhalconxl

SOURCES += \
    halcon_algorithm.cpp \
    image_processor.cpp \
    image_utils.cpp \
    image_view.cpp \
    logger.cpp \
    main.cpp \
    mainwindow.cpp \
    pipeline.cpp \
    pipeline_manager.cpp

HEADERS += \
    halcon_algorithm.h \
    image_processor.h \
    image_utils.h \
    image_view.h \
    logger.h \
    mainwindow.h \
    pipeline.h \
    pipeline_manager.h \
    pipeline_steps.h \
    shape_filter_types.h

FORMS += \
    mainwindow.ui

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target

DISTFILES +=

RESOURCES += \
    res.qrc
