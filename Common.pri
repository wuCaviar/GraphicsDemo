
PROJECT_PATH = D:/WorkSpace/Caviar/GraphicsDemo
DESTDIR = $$PROJECT_PATH/Bin
OBJECTS_DIR = $$PROJECT_PATH/Build/$$TARGET/obj
MOC_DIR = $$PROJECT_PATH/Build/$$TARGET/moc
RCC_DIR = $$PROJECT_PATH/Build/$$TARGET/rcc
UI_DIR = $$PROJECT_PATH/Build/$$TARGET/ui

# ---------- 第三方库----------
INCLUDEPATH += $$PROJECT_PATH/3rdParty/libtiff-msvc2022/include
LIBS += -L$$PROJECT_PATH/3rdParty/libtiff-msvc2022/lib \
        -ltiff

INCLUDEPATH += $$PROJECT_PATH/3rdParty/opencv/build/include
LIBS += -L$$PROJECT_PATH/3rdParty/opencv/build/x64/vc16/lib \
        -lopencv_world4120

INCLUDEPATH += $$PROJECT_PATH/3rdParty/lcms2-2.19/include
LIBS += -L$$PROJECT_PATH/3rdParty/lcms2-2.19/bin \
        -llcms2
