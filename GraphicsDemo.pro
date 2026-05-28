TEMPLATE = subdirs

SUBDIRS += \
    Libs \
    Src

Src.depends = Libs
