#ifndef PROJECTSERVICE_H
#define PROJECTSERVICE_H

#include "QAtService.h"

#include <QString>

// 项目文件状态管理服务 — 跟踪当前项目路径和修改状态
class ProjectService : public QAtService
{
    Q_OBJECT
public:
    explicit ProjectService(QObject *parent = nullptr);

    QString serviceId() const override { return QStringLiteral("project"); }
    QString description() const override;

    QString projectPath() const;
    void    setProjectPath(const QString &path);
    bool    isModified() const;
    void    setModified(bool m);

signals:
    void projectPathChanged(const QString &path);
    void projectModifiedChanged(bool modified);

private:
    QString m_path;
    bool    m_modified = false;
};

#endif // PROJECTSERVICE_H
