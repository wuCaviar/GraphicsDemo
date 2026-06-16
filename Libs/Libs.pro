TEMPLATE = subdirs

SUBDIRS += \
    ColorTrans \
    QtColorWidgets \
    QtGradientEditor \
    QSimpleUpdater \
    Layout \
    # Qtitan \
    QRuler

# 依赖顺序
ColorTrans.subdir = ColorTrans
QtColorWidgets.subdir = Qt-Color-Widgets
QtGradientEditor.subdir = QtGradientEditor
QSimpleUpdater.subdir = QSimpleUpdater
Layout.subdir = Layout
# Qtitan.subdir = Qtitan
QRuler.subdir = QRuler

QtColorWidgets.depends = ColorTrans
QtGradientEditor.depends = QtColorWidgets
