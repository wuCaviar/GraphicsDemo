#ifndef QATACTIONBASE_H
#define QATACTIONBASE_H

#include <QCommonDefs.h>
#include <QAction>

Q_CLASS_TYPEDEFS(QhsActionBase)
typedef QhsActionBasePtr(*ActionCreateFunc)();

class QhsActionFactory
{
protected:
	QhsActionFactory();

public:
	static QhsActionFactory& get();

	friend class _Register;

	class _Register
	{
	public:
		_Register(const QString& token, ActionCreateFunc func);
	};

public:
	QhsActionBasePtr createAction(const QString& token);

private:
	void _registerAction(const QString& token, ActionCreateFunc func);

protected:
	QMap<QString, ActionCreateFunc>		m_mapCreateFunc;
};

#define ENABLE_REGISTER_ACTION(class_name) \
class _Action##class_name \
{ \
	public: \
		static QhsActionBasePtr instance() \
		{ return QhsActionBasePtr(new class_name); } \
	private: \
		static const QhsActionFactory::_Register m_stRegister; \
};

#define REGISTER_ACTION(action_name, class_name) \
const QhsActionFactory::_Register class_name::_Action##class_name::m_stRegister( \
#action_name, class_name::_Action##class_name::instance);

class QhsActionBase
{
protected:
	QhsActionBase() {}

public:
	virtual ~QhsActionBase() {}

	friend class QhsActionFactory;

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

	void setStateCondition(const QString& condition)
	{
		m_strStateCondition = condition;
	}

	const QString& stateCondition() const
	{
		return m_strStateCondition;
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
	QString					m_strStateCondition;
	QVariant				m_valUserData;
};

#ifdef Q_OS_WIN
Q_DECLARE_METATYPE(QhsActionBasePtr)
#endif // Q_OS_WIN

#define ACTION_TOKEN(token) #token

#endif // QATACTIONBASE_H