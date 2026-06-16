TEMPLATE = subdirs

SUBDIRS += \
    QtitanBase \
    QtitanDocking \
    QtitanStyle 

# 依赖顺序
QtitanBase.subdir = $$PWD/src/shared/QtitanBase
QtitanDocking.subdir = $$PWD/src/shared/QtitanDocking
QtitanStyle.subdir = $$PWD/src/shared/QtitanStyle

QtitanStyle.depends = QtitanBase
QtitanDocking.depends = QtitanBase
