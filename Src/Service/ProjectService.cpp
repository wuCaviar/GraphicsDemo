#include "ProjectService.h"

ProjectService::ProjectService(QObject *parent) : QAtService(parent) {}

QString ProjectService::description() const { return tr("Project file state service"); }

QString ProjectService::projectPath() const           { return m_path; }
void    ProjectService::setProjectPath(const QString &path)
{
    if (m_path == path) return;
    m_path = path;
    emit projectPathChanged(m_path);
}

bool ProjectService::isModified() const               { return m_modified; }
void ProjectService::setModified(bool m)
{
    if (m_modified == m) return;
    m_modified = m;
    emit projectModifiedChanged(m_modified);
}
