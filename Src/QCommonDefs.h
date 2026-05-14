#ifndef QCOMMONDEFS_H
#define QCOMMONDEFS_H

#include <QtGlobal>
#include <QList>
#include <QMap>
#include <QSharedPointer>
#include <QStack>
#include <QString>
#include <QVector>
#include <QWeakPointer>

#define Q_CLASS_TYPEDEFS(_name_) \
    class _name_;                \
    typedef QSharedPointer<_name_> _name_##Ptr;

#define Q_FORWARD_DECLARE(_name_)               \
    class _name_;                               \
    typedef QSharedPointer<_name_> _name_##Ptr; \
    typedef QWeakPointer<_name_> _name_##WPtr;

#define Q_STATIC_CAST(_name_, _obj_) qSharedPointerCast<_name_>(_obj_)
#define Q_DYNAMIC_CAST(_name_, _obj_) qSharedPointerDynamicCast<_name_>(_obj_)
#define qmake_shared(_name_, ...) QSharedPointer<_name_>::create(__VA_ARGS__)

#define START_CLASS_PRIVATE(Class)                        \
    class Class;                                          \
    class Class##Private                                  \
    {                                                     \
    public:                                               \
        Class##Private(Class *parent) : q_ptr(parent) { } \
        ~Class##Private() { }                             \
                                                          \
    public:

#define ADD_PRIVATE_FIELD(type, pre, name)                       \
    type get##name() const { return m_##pre##name; }             \
    void set##name(const type &value) { m_##pre##name = value; } \
                                                                 \
protected:                                                       \
    type m_##pre##name;

#define END_CLASS_PRIVATE(Class) \
private:                         \
    Class *const q_ptr;          \
    Q_DECLARE_PUBLIC(Class);     \
    }                            \
    ;

#endif // QCOMMONDEFS_H
