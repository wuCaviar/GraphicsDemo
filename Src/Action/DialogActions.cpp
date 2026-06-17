#include "DialogActions.h"
#include "AppContext.h"
#include "AppConfig.h"
#include "AlignWidget.h"
#include "NetworkService.h"
#include "ProcessService.h"
#include "SettingsDialog.h"
#include "PreferencesDialog.h"
#include "atDefine.h"

#include <QMessageBox>
#include <QApplication>

QString SettingsAction::_text()
{
    return QObject::tr("&Settings");
}
QString PreferencesAction::_text()
{
    return QObject::tr("&Preferences");
}
QString AboutAction::_text()
{
    return QObject::tr("&About");
}
QString AlignWidgetAction::_text()
{
    return QObject::tr("&Align Layout");
}

void SettingsAction::_execute()
{
    SettingsDialog dlg(qApp->activeWindow());
    if (dlg.exec() != QDialog::Accepted)
        return;

    auto &ctx = AppContext::get();
    if (ctx.process()->isProcessRunning(AppConfig::instance().ripExePath()))
        ctx.network()->doRipVersion();
}
void PreferencesAction::_execute()
{
    PreferencesDialog dlg(qApp->activeWindow());
    dlg.exec();
}

void AboutAction::_execute()
{
    QString strText = QString(QObject::tr("<h3>AT Drawing Tools</h3>"
                                          "<p>Current Version: %1</p>"
                                          "<p>    Rip Version: %2</p>"))
                          .arg(qApp->applicationVersion())
                          .arg(RipVersion);

    QMessageBox::about(nullptr, QObject::tr("About AT Drawing Tools"), strText);
}

void AlignWidgetAction::_execute()
{
    auto *w = AppContext::get().alignWidget();
    if (!w)
        return;
    w->refreshSelectionInfo();
    w->show();
    w->raise();
}
