#include "AppConfig.h"

#include <QCoreApplication>
#include <QDebug>
#include <QDomDocument>
#include <QFile>
#include <QTextStream>

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

    // <ICC> 子元素
    QDomElement iccEl = root.firstChildElement(QStringLiteral("ICC"));
    if (!iccEl.isNull()) {
        QDomElement pathEl = iccEl.firstChildElement(QStringLiteral("Path"));
        if (!pathEl.isNull())
            m_iccProfileBasePath = pathEl.text().trimmed();
    }

    qInfo() << "[AppConfig] Loaded from config.xml"
            << "\n  ripExe:" << m_ripExePath
            << "\n  ripConfig:" << m_ripConfigPath
            << "\n  iccBase:" << iccProfileBasePath();
}

void AppConfig::saveConfig() const
{
    const QString cfgPath =
        QCoreApplication::applicationDirPath() + "/config.xml";

    QDomDocument doc;
    doc.appendChild(doc.createProcessingInstruction(
        QStringLiteral("xml"),
        QStringLiteral("version=\"1.0\" encoding=\"UTF-8\"")));

    QDomElement root = doc.createElement(QStringLiteral("Config"));
    doc.appendChild(root);

    // <Rip>
    QDomElement ripEl = doc.createElement(QStringLiteral("Rip"));
    root.appendChild(ripEl);

    QDomElement exeEl = doc.createElement(QStringLiteral("ExePath"));
    exeEl.appendChild(doc.createTextNode(m_ripExePath));
    ripEl.appendChild(exeEl);

    QDomElement cfgEl = doc.createElement(QStringLiteral("ConfigPath"));
    cfgEl.appendChild(doc.createTextNode(m_ripConfigPath));
    ripEl.appendChild(cfgEl);

    // <ICC>
    QDomElement iccEl = doc.createElement(QStringLiteral("ICC"));
    root.appendChild(iccEl);

    QDomElement pathEl = doc.createElement(QStringLiteral("Path"));
    pathEl.appendChild(doc.createTextNode(m_iccProfileBasePath));
    iccEl.appendChild(pathEl);

    QFile file(cfgPath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        qWarning() << "[AppConfig] Failed to write config.xml:" << cfgPath;
        return;
    }
    QTextStream ts(&file);
    doc.save(ts, 4);
    file.close();

    qInfo() << "[AppConfig] Saved config.xml";
}

QString AppConfig::iccProfileBasePath() const
{
    if (!m_iccProfileBasePath.isEmpty())
        return m_iccProfileBasePath;

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
    return iccProfileBasePath() + QStringLiteral("/RGB/SRGB IEC61966-2.1.icc");
}

QString AppConfig::cmykIccPath() const
{
    return iccProfileBasePath()
           + QStringLiteral("/CMYK/JapanColor2001Coated.icc");
}
