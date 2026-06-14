#include "DialogActions.h"
#include "AppContext.h"

#include <QMessageBox>
#include <QApplication>

QString SettingsAction::_text()           { return QObject::tr("&Settings"); }
QString PreferencesAction::_text()        { return QObject::tr("&Preferences"); }
QString AboutAction::_text()              { return QObject::tr("&About"); }
QString AlignLayoutDialogAction::_text()  { return QObject::tr("&Align Layout"); }

void SettingsAction::_execute()    { AppContext::get().invokeActionCallback(token()); }
void PreferencesAction::_execute() { AppContext::get().invokeActionCallback(token()); }

void AboutAction::_execute()
{
    QMessageBox::about(qApp->activeWindow(),
                       QObject::tr("About"),
                       QObject::tr("ATGraphics — Vector Graphics Editor"));
}

void AlignLayoutDialogAction::_execute() { AppContext::get().invokeActionCallback(token()); }
