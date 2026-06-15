#include "FileActions.h"
#include "AppContext.h"

#include <QApplication>
#include <QStyle>

// ==================== NewAction ====================
QIcon NewAction::_icon()
{
    return QApplication::style()->standardIcon(QStyle::SP_FileIcon);
}
QString NewAction::_text()
{
    return QObject::tr("&New");
}
QKeySequence NewAction::_shortcut() const
{
    return QKeySequence::New;
}
void NewAction::_execute()
{
    AppContext::get().invokeActionCallback(token());
}

// ==================== OpenProjectAction ====================
QIcon OpenProjectAction::_icon()
{
    return QApplication::style()->standardIcon(QStyle::SP_DialogOpenButton);
}
QString OpenProjectAction::_text()
{
    return QObject::tr("&Open");
}
QKeySequence OpenProjectAction::_shortcut() const
{
    return QKeySequence::Open;
}
void OpenProjectAction::_execute()
{
    AppContext::get().invokeActionCallback(token());
}

// ==================== SaveProjectAction ====================
QIcon SaveProjectAction::_icon()
{
    return QApplication::style()->standardIcon(QStyle::SP_DialogSaveButton);
}
QString SaveProjectAction::_text()
{
    return QObject::tr("&Save");
}
QKeySequence SaveProjectAction::_shortcut() const
{
    return QKeySequence::Save;
}
void SaveProjectAction::_execute()
{
    AppContext::get().invokeActionCallback(token());
}

// ==================== ImportImageAction ====================
QIcon ImportImageAction::_icon()
{
    return QIcon(QStringLiteral(":/icons/icons/file-import.svg"));
}
QString ImportImageAction::_text()
{
    return QObject::tr("&Import Image");
}
QKeySequence ImportImageAction::_shortcut() const
{
    return QKeySequence(Qt::CTRL | Qt::Key_I);
}
void ImportImageAction::_execute()
{
    AppContext::get().invokeActionCallback(token());
}

// ==================== ExportImageAction ====================
QIcon ExportImageAction::_icon()
{
    return QIcon(QStringLiteral(":/icons/icons/file-export.svg"));
}
QString ExportImageAction::_text()
{
    return QObject::tr("&Export Image");
}
QKeySequence ExportImageAction::_shortcut() const
{
    return QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_E);
}
void ExportImageAction::_execute()
{
    AppContext::get().invokeActionCallback(token());
}

// ==================== ExitAction ====================
QIcon ExitAction::_icon()
{
    return QApplication::style()->standardIcon(QStyle::SP_DialogCloseButton);
}
QString ExitAction::_text()
{
    return QObject::tr("E&xit");
}
QKeySequence ExitAction::_shortcut() const
{
    return QKeySequence::Quit;
}
void ExitAction::_execute()
{
    QApplication::quit();
}
