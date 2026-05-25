#include "AppConfig.h"

#include <QCoreApplication>
#include <QDebug>
#include <QDomDocument>
#include <QFile>

AppConfig &AppConfig::instance()
{
    static AppConfig s;
    return s;
}

void AppConfig::loadConfig()
{
    const QString cfgPath =
        QCoreApplication::applicationDirPath() + "/config.xml";
    QFile file(cfgPath);
    if (!file.open(QIODevice::ReadOnly)) {
        qInfo() << "[AppConfig] No config.xml found, using defaults";
        return;
    }

    QDomDocument doc;
    QString errMsg;
    int errLine, errCol;
    if (!doc.setContent(&file, &errMsg, &errLine, &errCol)) {
        qWarning() << "[AppConfig] XML parse error at line" << errLine
                   << "column" << errCol << ":" << errMsg;
        file.close();
        return;
    }
    file.close();

    QDomElement root = doc.documentElement();
    if (root.tagName() != QStringLiteral("Config")) {
        qWarning() << "[AppConfig] Unexpected root element:" << root.tagName();
        return;
    }

    // <Rip> 子元素
    QDomElement ripEl = root.firstChildElement(QStringLiteral("Rip"));
    if (!ripEl.isNull()) {
        QDomElement exeEl = ripEl.firstChildElement(QStringLiteral("ExePath"));
        if (!exeEl.isNull())
            m_ripExePath = exeEl.text();

        QDomElement cfgEl =
            ripEl.firstChildElement(QStringLiteral("ConfigPath"));
        if (!cfgEl.isNull())
            m_ripConfigPath = cfgEl.text();
    }

    qInfo() << "[AppConfig] Loaded from config.xml"
            << "\n  ripExe:" << m_ripExePath
            << "\n  ripConfig:" << m_ripConfigPath;
}

QString AppConfig::iccProfileBasePath() const
{
#if defined(Q_OS_WIN)
    return QCoreApplication::applicationDirPath();
#elif defined(Q_OS_MACOS)
    return QStringLiteral("/Volumes/Caviar/Test/GraphicsDemo/Bin");
#else
    return QCoreApplication::applicationDirPath();
#endif
}

QString AppConfig::srgbIccPath() const
{
    return iccProfileBasePath()
           + QStringLiteral("/ICC Profile/RGB/SRGB IEC61966-2.1.icc");
}

QString AppConfig::cmykIccPath() const
{
    return iccProfileBasePath()
           + QStringLiteral("/ICC Profile/CMYK/JapanColor2001Coated.icc");
}
