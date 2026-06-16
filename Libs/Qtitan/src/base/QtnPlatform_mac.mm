/****************************************************************************
**
** Qtitan Library by Developer Machines (Components for Qt.C++)
**
** Copyright (c) 2009-2021 Developer Machines (https://www.devmachines.com)
**           ALL RIGHTS RESERVED
**
**  The entire contents of this file is protected by copyright law and
**  international treaties. Unauthorized reproduction, reverse-engineering
**  and distribution of all or any portion of the code contained in this
**  file is strictly prohibited and may result in severe civil and
**  criminal penalties and will be prosecuted to the maximum extent
**  possible under the law.
**
**  RESTRICTIONS
**
**  THE SOURCE CODE CONTAINED WITHIN THIS FILE AND ALL RELATED
**  FILES OR ANY PORTION OF ITS CONTENTS SHALL AT NO TIME BE
**  COPIED, TRANSFERRED, SOLD, DISTRIBUTED, OR OTHERWISE MADE
**  AVAILABLE TO OTHER INDIVIDUALS WITHOUT WRITTEN CONSENT
**  AND PERMISSION FROM DEVELOPER MACHINES
**
**  CONSULT THE END USER LICENSE AGREEMENT FOR INFORMATION ON
**  ADDITIONAL RESTRICTIONS.
**
****************************************************************************/

#include "QtnPlatform.h"

#include <QApplication>
#include <QWidget>
#include <QWindow>

#import <AppKit/AppKit.h>

#ifdef QTN_MEMORY_DEBUG
#include "QtitanMSVSDebug.h"
#endif

QTITAN_USE_NAMESPACE

bool QTITAN_PREPEND_NAMESPACE(qtn_set_window_hook)(QWidget* w)
{
    Q_UNUSED(w);
    return false;
}

bool QTITAN_PREPEND_NAMESPACE(qtn_unset_window_hook)(QWidget* w, bool restore)
{
    Q_UNUSED(w);
    Q_UNUSED(restore);
    return false;
}

bool QTITAN_PREPEND_NAMESPACE(qtn_set_window_frameless_mac)(QWidget* w)
{
    if (!w)
        return false;

    NSView* view = reinterpret_cast<NSView*>(w->winId());
    if (!view)
        return false;

    NSWindow* window = [view window];
    if (!window)
        return false;

    [window setStyleMask:[window styleMask] | NSWindowStyleMaskFullSizeContentView];
    [window setTitlebarAppearsTransparent:YES];
    [window setMovableByWindowBackground:YES];

    return true;
}

void QTITAN_PREPEND_NAMESPACE(qtn_paint_title_bar_mac)(const QStyleOptionComplex* option, QPainter* painter,
    const QWidget* widget)
{
    Q_UNUSED(option);
    Q_UNUSED(painter);
    Q_UNUSED(widget);
}

bool QTITAN_PREPEND_NAMESPACE(qtn_window_start_native_drag_mac)(QWidget* w, const QPoint& pos)
{
    Q_UNUSED(w);
    Q_UNUSED(pos);
    return false;
}

void QTITAN_PREPEND_NAMESPACE(qtn_window_resize_mac)(QWidget* w)
{
    Q_UNUSED(w);
}

QPixmap QTITAN_PREPEND_NAMESPACE(qtn_titlebar_standard_pixmap)(QStyle::StandardPixmap sp,
    const QStyleOption* opt, const QWidget* widget)
{
    Q_UNUSED(sp);
    Q_UNUSED(opt);
    Q_UNUSED(widget);
    return QPixmap();
}

Qt::MouseButtons QTITAN_PREPEND_NAMESPACE(qtn_get_mouseButtons_mac)()
{
    return QApplication::mouseButtons();
}

void QTITAN_PREPEND_NAMESPACE(qtn_setWidgetPostion)(QWidget* widget, const QPoint& position)
{
    if (widget)
        widget->move(position);
}

bool QTITAN_PREPEND_NAMESPACE(qtn_setBlurBehindWindow)(QWidget* widget, bool enabled)
{
    Q_UNUSED(widget);
    Q_UNUSED(enabled);
    return false;
}

void QTITAN_PREPEND_NAMESPACE(qtn_paintAirEffect)(QPainter* painter, const QRect& rect,
    const QRegion& clip)
{
    Q_UNUSED(painter);
    Q_UNUSED(rect);
    Q_UNUSED(clip);
}

void QTITAN_PREPEND_NAMESPACE(qtn_paintTitleBarText)(QPainter* painter, const QString& text,
    const QRect& rect, bool active, const QColor& color)
{
    Q_UNUSED(painter);
    Q_UNUSED(text);
    Q_UNUSED(rect);
    Q_UNUSED(active);
    Q_UNUSED(color);
}

QPixmap QTITAN_PREPEND_NAMESPACE(qtn_getTitleBarIcon)(QWidget* widget)
{
    Q_UNUSED(widget);
    return QPixmap();
}

void QTITAN_PREPEND_NAMESPACE(qtn_paintTitleBarIcon)(QPainter* painter, const QIcon& icon,
    const QRect& rect)
{
    Q_UNUSED(painter);
    Q_UNUSED(icon);
    Q_UNUSED(rect);
}

QImage QTITAN_PREPEND_NAMESPACE(qtn_getDesktopImage)(int screen)
{
    Q_UNUSED(screen);
    return QImage();
}

DesktopImageAspectStyle QTITAN_PREPEND_NAMESPACE(qtn_getDesktopAspectStyle)(int screen)
{
    Q_UNUSED(screen);
    DesktopImageAspectStyle ret = AspectStyleCentral;
    return ret;
}
