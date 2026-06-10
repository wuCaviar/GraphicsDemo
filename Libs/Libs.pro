TEMPLATE = subdirs

SUBDIRS += \
    ColorTrans \
    QtColorWidgets \
    QtGradientEditor \
    QSimpleUpdater \
    Layout

# 依赖顺序
ColorTrans.subdir = ColorTrans
QtColorWidgets.subdir = Qt-Color-Widgets
QtGradientEditor.subdir = QtGradientEditor
QSimpleUpdater.subdir = QSimpleUpdater
Layout.subdir = Layout

QtColorWidgets.depends = ColorTrans
QtGradientEditor.depends = QtColorWidgets
