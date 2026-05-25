
INCLUDEPATH += $$PWD/src $$PWD/include

SOURCES += \
    $$PWD/src/AuthenticateDialog.cpp \
    $$PWD/src/Downloader.cpp \
    $$PWD/src/QSimpleUpdater.cpp \
    $$PWD/src/Updater.cpp \

HEADERS += \
    $$PWD/include/QSimpleUpdater.h \
    $$PWD/src/AuthenticateDialog.h \
    $$PWD/src/Downloader.h \
    $$PWD/src/Updater.h \

FORMS += \
    $$PWD/src/Downloader.ui \
    $$PWD/src/AuthenticateDialog.ui \

RESOURCES += \
    $$PWD/etc/qsimpleupdater.qrc