#include "ViewActions.h"
#include "AppContext.h"
#include "QAtCanvasPage.h"
#include "qatgraphicsview.h"
#include "ThemeService.h"

#include <QApplication>
#include <QStyle>

// ==================== ToggleGridAction ====================
QString ToggleGridAction::_text() { return QObject::tr("Show &Grid"); }
QKeySequence ToggleGridAction::_shortcut() const { return QKeySequence(Qt::CTRL | Qt::Key_G); }
void ToggleGridAction::_execute()
{
    auto *v = view();
    if (v) v->setGridVisible(!v->isGridVisible());
}
void ToggleGridAction::_onUpdateState(bool &enabled, bool &checked, bool &)
{
    auto *v = view();
    enabled = v != nullptr;
    checked = v && v->isGridVisible();
}

// ==================== FitToCanvasAction ====================
QString FitToCanvasAction::_text() { return QObject::tr("&Fit to Canvas"); }
void FitToCanvasAction::_execute()
{
    auto *v = view();
    if (v) v->fitToCanvas();
}

// ==================== ResetZoomAction ====================
QString ResetZoomAction::_text() { return QObject::tr("&Reset Zoom"); }
QKeySequence ResetZoomAction::_shortcut() const { return QKeySequence(Qt::CTRL | Qt::Key_0); }
void ResetZoomAction::_execute()
{
    auto *v = view();
    if (v) v->setZoomLevel(1.0);
}

// ==================== ThemeAction ====================
QString ThemeAction::_text() { return QObject::tr("&Light Theme"); }
void ThemeAction::_execute()
{
    auto *ts = AppContext::get().theme();
    if (!ts) return;
    QString current = ts->currentTheme();
    ts->setTheme(current == QStringLiteral("light")
                     ? QStringLiteral("dark")
                     : QStringLiteral("light"));
}
