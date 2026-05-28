
PROJECT_PATH = $$PWD
DESTDIR = $$PROJECT_PATH/Bin
OBJECTS_DIR = $$PROJECT_PATH/Build/$$TARGET/obj
MOC_DIR = $$PROJECT_PATH/Build/$$TARGET/moc
RCC_DIR = $$PROJECT_PATH/Build/$$TARGET/rcc
UI_DIR = $$PROJECT_PATH/Build/$$TARGET/ui

# 跨平台外部依赖路径
win32 {
    # Windows: 假设 vcpkg 安装在用户目录，或通过环境变量配置
    TIFF_ROOT = $$(TIFF_ROOT)
    LCMS2_ROOT = $$(LCMS2_ROOT)

    isEmpty(TIFF_ROOT): TIFF_ROOT = C:/vcpkg/installed/x64-windows
    isEmpty(LCMS2_ROOT): LCMS2_ROOT = C:/vcpkg/installed/x64-windows

    INCLUDEPATH += $$TIFF_ROOT/include $$LCMS2_ROOT/include
    LIBS += -L$$TIFF_ROOT/lib -L$$LCMS2_ROOT/lib
} else:macx {
    # macOS: Homebrew (ARM)
    INCLUDEPATH += /opt/homebrew/include
    LIBS += -L/opt/homebrew/lib
} else:unix {
    # Linux: 系统路径
    INCLUDEPATH += /usr/include /usr/local/include
    LIBS += -L/usr/lib -L/usr/local/lib
}

LIBS += -ltiff -llcms2
