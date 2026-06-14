#ifndef THEMESERVICE_H
#define THEMESERVICE_H

#include "QAtService.h"

#include <QString>
#include <QStringList>

// 主题管理服务 — 加载、切换、持久化应用主题
class ThemeService : public QAtService
{
    Q_OBJECT

public:
    explicit ThemeService(QObject *parent = nullptr);

    QString serviceId() const override { return QStringLiteral("theme"); }
    QString description() const override;

    bool initialize() override;
    void shutdown() override;

    QString currentTheme() const;
    void    setTheme(const QString &name);
    QStringList availableThemes() const;

signals:
    void themeChanged(const QString &name);

private:
    void loadThemeFile(const QString &name);

    QString m_currentTheme;
    QStringList m_availableThemes;
};

#endif // THEMESERVICE_H
