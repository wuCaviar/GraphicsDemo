#ifndef VIEWACTIONS_H
#define VIEWACTIONS_H

#include "QAtViewAction.h"

class ToggleGridAction : public QAtViewActionBase
{
public:
    ToggleGridAction() { m_strActionToken = QStringLiteral("ToggleGrid"); m_checkable = true; }
    QString _text() override;
    QKeySequence _shortcut() const override;
    void _execute() override;
    void _onUpdateState(bool &enabled, bool &checked, bool &) override;
};

class FitToCanvasAction : public QAtViewActionBase
{
public:
    FitToCanvasAction() { m_strActionToken = QStringLiteral("FitToCanvas"); }
    QString _text() override;
    void _execute() override;
};

class ResetZoomAction : public QAtViewActionBase
{
public:
    ResetZoomAction() { m_strActionToken = QStringLiteral("ResetZoom"); }
    QString _text() override;
    QKeySequence _shortcut() const override;
    void _execute() override;
};

class ThemeAction : public QAtViewActionBase
{
public:
    ThemeAction() { m_strActionToken = QStringLiteral("LightTheme"); m_checkable = true; }
    QString _text() override;
    void _execute() override;
};

#endif // VIEWACTIONS_H
