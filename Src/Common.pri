

# ---------- 3. 第三方库----------

# libtiff（macOS Homebrew）
INCLUDEPATH += /opt/homebrew/include
LIBS += -L/opt/homebrew/lib -ltiff

# OpenCV — 当前代码未使用，暂不链接
# INCLUDEPATH += /opt/homebrew/include/opencv4
# LIBS += -L/opt/homebrew/lib -lopencv_core -lopencv_imgproc -lopencv_imgcodecs

# QtColorWidgets — 已用 Qt 内置 QColorDialog 替代，不再依赖
