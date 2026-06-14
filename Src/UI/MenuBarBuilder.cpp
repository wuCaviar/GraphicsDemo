#include "MenuBarBuilder.h"
#include "AppContext.h"

#include <QMenuBar>
#include <QMenu>
#include <QAction>

MenuBarBuilder::MenuBarBuilder(QMenuBar *menuBar, QObject *parent)
    : QObject(parent), m_menuBar(menuBar)
{
}

void MenuBarBuilder::build()
{
    m_menuBar->clear();
    buildFileMenu();
    buildEditMenu();
    buildArrangeMenu();
    buildViewMenu();
    buildSettingsMenu();
    buildHelpMenu();
}

void MenuBarBuilder::buildFileMenu()
{
    auto *menu = buildMenuFromCategory(tr("&File"), QStringLiteral("File"));
    m_menuBar->addMenu(menu);
}

void MenuBarBuilder::buildEditMenu()
{
    auto *menu = buildMenuFromCategory(tr("&Edit"), QStringLiteral("Edit"));
    connect(menu, &QMenu::aboutToShow, []() {
        AppContext::get().refreshAllActions();
    });
    m_menuBar->addMenu(menu);
}

void MenuBarBuilder::buildArrangeMenu()
{
    auto *menu = buildMenuFromCategory(tr("&Arrange"), QStringLiteral("Arrange"));
    m_menuBar->addMenu(menu);
}

void MenuBarBuilder::buildViewMenu()
{
    auto *menu = buildMenuFromCategory(tr("&View"), QStringLiteral("View"));
    m_menuBar->addMenu(menu);
}

void MenuBarBuilder::buildSettingsMenu()
{
    auto *menu = buildMenuFromCategory(tr("&Settings"), QStringLiteral("Settings"));
    m_menuBar->addMenu(menu);
}

void MenuBarBuilder::buildHelpMenu()
{
    auto *menu = m_menuBar->addMenu(tr("&Help"));
    QAction *aboutAct = AppContext::get().getQAction(QStringLiteral("About"));
    if (aboutAct) menu->addAction(aboutAct);
}

QMenu* MenuBarBuilder::buildMenuFromCategory(const QString &title, const QString &category)
{
    auto *menu = new QMenu(title, m_menuBar);
    auto actions = AppContext::get().actionsByCategory(category);
    for (const auto &a : actions) {
        QAction *qa = AppContext::get().getQAction(a->token());
        if (qa) menu->addAction(qa);
    }
    return menu;
}
