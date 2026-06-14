#ifndef MENUBARBUILDER_H
#define MENUBARBUILDER_H

#include <QObject>

class QMenuBar;
class QMenu;

// Builds menu bar declaratively from AppContext-registered Actions
class MenuBarBuilder : public QObject
{
    Q_OBJECT
public:
    explicit MenuBarBuilder(QMenuBar *menuBar, QObject *parent = nullptr);

    void build();

private:
    void buildFileMenu();
    void buildEditMenu();
    void buildArrangeMenu();
    void buildViewMenu();
    void buildSettingsMenu();
    void buildHelpMenu();

    QMenu* buildMenuFromCategory(const QString &title, const QString &category);

    QMenuBar *m_menuBar;
};

#endif // MENUBARBUILDER_H
