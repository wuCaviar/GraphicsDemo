#ifndef FILEACTIONS_H
#define FILEACTIONS_H

#include "QAtFileAction.h"
#include "QAtActionBase.h"

// File actions — category "File", global scope
class NewAction : public QAtFileActionBase
{
public:
    NewAction() { m_strActionToken = QStringLiteral("New"); }
    QIcon _icon() override;
    QString _text() override;
    QKeySequence _shortcut() const override;
    void _execute() override;
};

class OpenProjectAction : public QAtFileActionBase
{
public:
    OpenProjectAction() { m_strActionToken = QStringLiteral("OpenProject"); }
    QIcon _icon() override;
    QString _text() override;
    QKeySequence _shortcut() const override;
    void _execute() override;
};

class SaveProjectAction : public QAtFileActionBase
{
public:
    SaveProjectAction() { m_strActionToken = QStringLiteral("SaveProject"); }
    QIcon _icon() override;
    QString _text() override;
    QKeySequence _shortcut() const override;
    void _execute() override;
};

class ImportImageAction : public QAtFileActionBase
{
public:
    ImportImageAction()
    {
        m_strActionToken = QStringLiteral("ImportImage");
        m_pageTypes = { QStringLiteral("canvas") };
    }
    QIcon _icon() override;
    QString _text() override;
    QKeySequence _shortcut() const override;
    void _execute() override;
};

class ExportImageAction : public QAtFileActionBase
{
public:
    ExportImageAction()
    {
        m_strActionToken = QStringLiteral("ExportImage");
        m_pageTypes = { QStringLiteral("canvas") };
    }
    QIcon _icon() override;
    QString _text() override;
    QKeySequence _shortcut() const override;
    void _execute() override;
};

class ExitAction : public QAtFileActionBase
{
public:
    ExitAction() { m_strActionToken = QStringLiteral("Exit"); }
    QIcon _icon() override;
    QString _text() override;
    QKeySequence _shortcut() const override;
    void _execute() override;
};

#endif // FILEACTIONS_H
