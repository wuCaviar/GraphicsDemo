
# ============================================================
# Common.pri — 所有 subdirs 共享的输出路径和第三方依赖配置
# ============================================================

PROJECT_PATH = $$PWD

# === Debug / Release 输出分离 ===
CONFIG(debug, debug|release) {
    OUT_SUBDIR = Debug
} else {
    OUT_SUBDIR = Release∑
}

DESTDIR     = $$PROJECT_PATH/Bin/$$OUT_SUBDIR
OBJECTS_DIR = $$PROJECT_PATH/Build/$$OUT_SUBDIR/$$TARGET/obj
MOC_DIR     = $$PROJECT_PATH/Build/$$OUT_SUBDIR/$$TARGET/moc
RCC_DIR     = $$PROJECT_PATH/Build/$$OUT_SUBDIR/$$TARGET/rcc
UI_DIR      = $$PROJECT_PATH/Build/$$OUT_SUBDIR/$$TARGET/ui

# === 第三方库根路径（支持环境变量覆盖） ===
win32 {
    OPENCV_ROOT = $$(OPENCV_ROOT)
    TIFF_ROOT   = $$(TIFF_ROOT)
    LCMS2_ROOT  = $$(LCMS2_ROOT)

    isEmpty(OPENCV_ROOT): OPENCV_ROOT = D:/WorkSpace/opencv/build
    isEmpty(TIFF_ROOT):   TIFF_ROOT   = $$PROJECT_PATH/3rdParty/libtiff-msvc2022
    isEmpty(LCMS2_ROOT):  LCMS2_ROOT  = $$PROJECT_PATH/3rdParty/lcms2-2.19

    INCLUDEPATH += $$TIFF_ROOT/include $$LCMS2_ROOT/include
    LIBS += -L$$TIFF_ROOT/lib -L$$LCMS2_ROOT/bin
} else:macx {
    INCLUDEPATH += /opt/homebrew/include
    LIBS += -L/opt/homebrew/lib
} else:unix {
    INCLUDEPATH += /usr/include /usr/local/include
    LIBS += -L/usr/lib -L/usr/local/lib
}

LIBS += -ltiff -llcms2
