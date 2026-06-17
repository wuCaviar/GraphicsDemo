#ifndef DIALOGACTIONS_H
#define DIALOGACTIONS_H

#include "QAtDialogAction.h"

class SettingsAction : public QAtDialogActionBase
{
public:
    SettingsAction() { m_strActionToken = QStringLiteral("Settings"); }
    QString _text() override;
    void _execute() override;
};

class PreferencesAction : public QAtDialogActionBase
{
public:
    PreferencesAction() { m_strActionToken = QStringLiteral("Preferences"); }
    QString _text() override;
    void _execute() override;
};

class AboutAction : public QAtDialogActionBase
{
public:
    AboutAction() { m_strActionToken = QStringLiteral("About"); }
    QString _text() override;
    void _execute() override;
};

class AlignWidgetAction : public QAtDialogActionBase
{
public:
    AlignWidgetAction()
    {
        m_strActionToken = QStringLiteral("AlignLayout");
        m_checkable = true;
    }
    QString _text() override;
    void _execute() override;
};

#endif // DIALOGACTIONS_H
