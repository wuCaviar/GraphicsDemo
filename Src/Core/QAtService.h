#ifndef QATSERVICE_H
#define QATSERVICE_H

#include <QObject>
#include <QString>
#include <QStringList>

// 所有全局服务的抽象基类
// 统一生命周期管理：initialize() → use → shutdown()
class QAtService : public QObject
{
    Q_OBJECT

public:
    explicit QAtService(QObject *parent = nullptr) : QObject(parent) { }
    virtual ~QAtService() = default;

    virtual QString serviceId() const = 0;
    virtual QString description() const { return { }; }

    virtual bool initialize() { return true; }
    virtual void shutdown() { }

    // 依赖的其他 service ID，框架按拓扑顺序初始化
    virtual QStringList dependencies() const { return { }; }
};

#endif // QATSERVICE_H
