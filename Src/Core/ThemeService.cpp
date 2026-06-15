#include "ThemeService.h"

#include <QApplication>
#include <QFile>
#include <QSettings>

ThemeService::ThemeService(QObject *parent) : QAtService(parent) { }

QString ThemeService::description() const
{
    return tr("Theme management service");
}

bool ThemeService::initialize()
{
    m_availableThemes.clear();
    m_availableThemes << QStringLiteral("light") << QStringLiteral("dark");

    // Restore persisted theme, default to light
    QSettings settings;
    m_currentTheme =
        settings.value(QStringLiteral("appearance/theme"), QStringLiteral("light")).toString();
    loadThemeFile(m_currentTheme);
    return true;
}

void ThemeService::shutdown() { }

QString ThemeService::currentTheme() const
{
    return m_currentTheme;
}

void ThemeService::setTheme(const QString &name)
{
    if (m_currentTheme == name)
        return;
    if (!m_availableThemes.contains(name))
        return;

    m_currentTheme = name;
    loadThemeFile(name);
    QSettings().setValue(QStringLiteral("appearance/theme"), name);
    emit themeChanged(name);
}

QStringList ThemeService::availableThemes() const
{
    return m_availableThemes;
}

void ThemeService::loadThemeFile(const QString &name)
{
    // Matches existing qrc prefix: qdarkstyle/<theme>/<theme>style.qss
    QString qssPath = QStringLiteral(":/qdarkstyle/%1/%2style.qss").arg(name, name);
    QFile file(qssPath);
    if (file.open(QFile::ReadOnly | QFile::Text)) {
        QString stylesheet = QString::fromUtf8(file.readAll());
        qApp->setStyleSheet(stylesheet);
        file.close();
    }
}
