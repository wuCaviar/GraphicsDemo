#ifndef QATDRAWACTION_H
#define QATDRAWACTION_H

#include "QAtActionBase.h"

#include <QGraphicsView>
#include <QGraphicsScene>

// 绘图动作功能基类
class QAtDrawActionBase : public QAtActionBase
{
protected:
    QAtDrawActionBase();

public:
    virtual ~QAtDrawActionBase() { }

public:
    void setAttchViewScene(QGraphicsView *view, QGraphicsScene *scene);

protected:
    virtual void _onUpdateState(bool &isEnabled, bool &isChecked,
                                bool &isVisible) override;

    virtual void _execute() override;

protected:
    QGraphicsView *m_pView;
    QGraphicsScene *m_pScene;
};

class QAtDrawRectAction : public QAtDrawActionBase
{
public:
    QAtDrawRectAction();

    ~QAtDrawRectAction() { }

protected:
    virtual void _onUpdateState(bool &isEnabled, bool &isChecked,
                                bool &isVisible) override;

    virtual void _execute() override;
};

#endif // QATDRAWACTION_H