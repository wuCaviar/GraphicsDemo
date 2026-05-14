
PROJECT_PATH = /Volumes/Caviar/Test/GraphicsDemo
DESTDIR = $$PROJECT_PATH/Bin
OBJECTS_DIR = $$PROJECT_PATH/Build/$$TARGET/obj
MOC_DIR = $$PROJECT_PATH/Build/$$TARGET/moc
RCC_DIR = $$PROJECT_PATH/Build/$$TARGET/rcc
UI_DIR = $$PROJECT_PATH/Build/$$TARGET/ui

INCLUDEPATH += /opt/homebrew/include
LIBS += -L/opt/homebrew/lib \
    -ltiff \
    -llcms2
