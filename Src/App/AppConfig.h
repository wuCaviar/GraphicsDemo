#ifndef APPCONFIG_H
#define APPCONFIG_H

#include <QString>

class AppConfig
{
public:
    static AppConfig &instance();

    // 从 config.xml 加载配置，加载失败保留默认值
    void loadConfig();

    // 保存当前配置到 config.xml
    void saveConfig() const;

    // -- RIP 可执行文件路径 (config.xml: /Config/Rip/ExePath) --
    QString ripExePath() const { return m_ripExePath; }
    void setRipExePath(const QString &path) { m_ripExePath = path; }

    // -- RIP 配置文件路径 (config.xml: /Config/Rip/ConfigPath) --
    QString ripConfigPath() const { return m_ripConfigPath; }
    void setRipConfigPath(const QString &path) { m_ripConfigPath = path; }

    // -- ICC Profile 基础目录 (config.xml: /Config/ICC/Path) --
    // 未配置时回退: Windows = applicationDirPath(), macOS = /Volumes/Caviar/Test/GraphicsDemo/Bin
    QString iccProfileBasePath() const;
    void setIccProfileBasePath(const QString &path) { m_iccProfileBasePath = path; }

    // -- 具体 ICC 文件路径 --
    QString srgbIccPath() const;
    QString cmykIccPath() const;
    QString grayIccPath() const;

    // -- RIP 配置中的 ICC (从 ripconfig.xml 读取，SettingsDialog 维护) --
    QString dotCurveIccPath() const { return m_dotCurveIccPath; }
    void setDotCurveIccPath(const QString &path) { m_dotCurveIccPath = path; }

    QString proofIccPath() const { return m_proofIccPath; }
    void setProofIccPath(const QString &path) { m_proofIccPath = path; }

    // -- RIP 分辨率 (从 ripconfig.xml 读取) --
    int ripResolutionX() const { return m_ripResolutionX; }
    void setRipResolutionX(int x) { m_ripResolutionX = x; }
    int ripResolutionY() const { return m_ripResolutionY; }
    void setRipResolutionY(int y) { m_ripResolutionY = y; }

    // -- 网络服务根地址 --
    QString networkRoot() const { return m_networkRoot; }
    void setNetworkRoot(const QString &url) { m_networkRoot = url; }

    // -- 画布留白 (单位 mm，config.xml: /Config/Canvas/Margin) --
    double canvasMarginLeft() const { return m_canvasMarginLeft; }
    double canvasMarginRight() const { return m_canvasMarginRight; }
    double canvasMarginTop() const { return m_canvasMarginTop; }
    double canvasMarginBottom() const { return m_canvasMarginBottom; }
    void setCanvasMarginLeft(double v) { m_canvasMarginLeft = v; }
    void setCanvasMarginRight(double v) { m_canvasMarginRight = v; }
    void setCanvasMarginTop(double v) { m_canvasMarginTop = v; }
    void setCanvasMarginBottom(double v) { m_canvasMarginBottom = v; }

    // -- 图片缩略图缓存 --
    QString cachePath() const { return m_cachePath; }
    void setCachePath(const QString &path) { m_cachePath = path; }
    qint64 maxCacheSizeBytes() const { return m_maxCacheSizeBytes; }
    void setMaxCacheSizeBytes(qint64 bytes) { m_maxCacheSizeBytes = bytes; }

private:
    AppConfig() = default;
    ~AppConfig() = default;
    Q_DISABLE_COPY(AppConfig)

    QString m_ripExePath;
    QString m_ripConfigPath;
    QString m_iccProfileBasePath;
    QString m_dotCurveIccPath;
    QString m_proofIccPath;
    int m_ripResolutionX = 300;
    int m_ripResolutionY = 300;
    QString m_networkRoot = QStringLiteral("http://127.0.0.1:9201");
    double m_canvasMarginLeft = 6.35;
    double m_canvasMarginRight = 6.35;
    double m_canvasMarginTop = 5.08;
    double m_canvasMarginBottom = 5.08;

    // 图片缩略图缓存
    QString m_cachePath; // 空 = 使用 QStandardPaths::CacheLocation
    qint64 m_maxCacheSizeBytes = 500LL * 1024 * 1024; // 500 MiB
};

#endif // APPCONFIG_H
