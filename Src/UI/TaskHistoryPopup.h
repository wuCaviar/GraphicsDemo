#ifndef TASKHISTORYPOPUP_H
#define TASKHISTORYPOPUP_H

#include <QFrame>
#include <QVBoxLayout>
#include "ProgressManager.h"

class QLabel;

// 任务历史弹出窗口 — 在按钮上方弹出，显示进行中和已完成任务
class TaskHistoryPopup : public QFrame
{
    Q_OBJECT
public:
    explicit TaskHistoryPopup(QWidget *parent = nullptr);

    // 用 ProgressManager 数据刷新列表
    void refresh(const QList<ProgressManager::TaskInfo> &activeTasks,
                 const QList<ProgressManager::TaskInfo> &finishedTasks);

    // 在指定控件上方定位并显示
    void showAbove(QWidget *anchor,
                   const QList<ProgressManager::TaskInfo> &activeTasks,
                   const QList<ProgressManager::TaskInfo> &finishedTasks);

signals:
    void taskClicked(const QString &taskId);
    void popupHidden();

protected:
    void focusOutEvent(QFocusEvent *event) override;
    bool eventFilter(QObject *obj, QEvent *event) override;

private:
    void _clearLayout();
    QWidget *_createTaskRow(const ProgressManager::TaskInfo &info);

    QVBoxLayout *m_layout = nullptr;
};

#endif // TASKHISTORYPOPUP_H
