TEMPLATE = lib
CONFIG += shared c++17
DEFINES += LAYOUT_LIBRARY _HAS_STD_BYTE=0

TARGET = Layout

include(../../Common.pri)

INCLUDEPATH += $$PWD/..
INCLUDEPATH += $$OPENCV_ROOT/include

HEADERS += \
    opencvtest.h \
    exif.h \
    jconfig.h \
    jerror.h \
    jmorecfg.h \
    jpeglib.h

SOURCES += \
    opencvtest.cpp \
    exif.cpp

# OpenCV（Debug/Release 自动选择）
LIBS += -L$$OPENCV_ROOT/x64/vc16/lib
CONFIG(debug, debug|release) {
    LIBS += -lopencv_world4100d
} else {
    LIBS += -lopencv_world4100
}

# libjpeg-turbo（静态库）+ GDI+
# libtiff 已在 Common.pri 中统一链接
LIBS += -L$$PROJECT_PATH/3rdParty
LIBS += -llibjpeg-turbo -lgdiplus
