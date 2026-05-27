#ifndef QATACTIONBASE_H
#define QATACTIONBASE_H

#include <QCommonDefs.h>
#include <QAction>

Q_CLASS_TYPEDEFS(QAtActionBase)
typedef QAtActionBasePtr(*ActionCreateFunc)();

class QAtActionFactory
{
protected:
	QAtActionFactory();

public:
	static QAtActionFactory& get();

	friend class _Register;

	class _Register
	{
	public:
		_Register(const QString& token, ActionCreateFunc func);
	};

public:
	QAtActionBasePtr createAction(const QString& token);

private:
	void _registerAction(const QString& token, ActionCreateFunc func);

protected:
	QMap<QString, ActionCreateFunc>		m_mapCreateFunc;
};

#define ENABLE_REGISTER_ACTION(class_name) \
class _Action##class_name \
{ \
	public: \
		static QAtActionBasePtr instance() \
		{ return QAtActionBasePtr(new class_name); } \
	private: \
		static const QAtActionFactory::_Register m_stRegister; \
};

#define REGISTER_ACTION(action_name, class_name) \
const QAtActionFactory::_Register class_name::_Action##class_name::m_stRegister( \
#action_name, class_name::_Action##class_name::instance);

class QAtActionBase
{
protected:
	QAtActionBase() {}

public:
	virtual ~QAtActionBase() {}

	friend class QAtActionFactory;

public:
	const QString& token() const
	{
		return m_strActionToken;
	}

	QIcon icon()
	{
		return _icon();
	}

	QString text()
	{
		return _text();
	}

	void onUpdateState(bool& isEnabled, bool& isChecked, bool& isVisible);

	void execute()
	{
		_execute();
	}

	void setUserData(const QVariant& data)
	{
		m_valUserData = data;
	}

	const QVariant& userData() const
	{
		return m_valUserData;
	}

protected:
	virtual QIcon _icon() { return QIcon(); }

	virtual QString _text() { return QString(); }

	virtual void _onUpdateState(bool& isEnabled,
		bool& isChecked, bool& isVisible);

	virtual void _execute() = 0;

protected:
	QString					m_strActionToken;
	QVariant				m_valUserData;

};

#ifdef Q_OS_WIN
Q_DECLARE_METATYPE(QAtActionBasePtr)
#endif // Q_OS_WIN

#define ACTION_TOKEN(token) #token

#endif // QATACTIONBASE_H