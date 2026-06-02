#include "TaskHistoryPopup.h"

#include <QLabel>
#include <QHBoxLayout>
#include <QMouseEvent>
#include <QApplication>
#include <QScreen>
#include <QGraphicsDropShadowEffect>

static constexpr int kRowHeight = 32;
static constexpr int kPopupMaxWidth = 360;
static constexpr int kIconSize = 16;

TaskHistoryPopup::TaskHistoryPopup(QWidget *parent)
    : QFrame(parent, Qt::Popup | Qt::FramelessWindowHint)
{
    setAttribute(Qt::WA_ShowWithoutActivating);
    setAttribute(Qt::WA_DeleteOnClose, false);

    m_layout = new QVBoxLayout(this);
    m_layout->setContentsMargins(4, 4, 4, 4);
    m_layout->setSpacing(0);

    setStyleSheet(QStringLiteral("TaskHistoryPopup {"
                                 "  background-color: #ffffff;"
                                 "  border: 1px solid #c0c0c0;"
                                 "  border-radius: 4px;"
                                 "}"));

    auto *shadow = new QGraphicsDropShadowEffect(this);
    shadow->setBlurRadius(12);
    shadow->setColor(QColor(0, 0, 0, 60));
    shadow->setOffset(0, 2);
    setGraphicsEffect(shadow);

    setMinimumWidth(200);
    setMaximumWidth(kPopupMaxWidth);
}

void TaskHistoryPopup::refresh(
    const QList<ProgressManager::TaskInfo> &activeTasks,
    const QList<ProgressManager::TaskInfo> &finishedTasks)
{
    _clearLayout();

    for (const auto &info : activeTasks)
        m_layout->addWidget(_createTaskRow(info));

    if (!activeTasks.isEmpty() && !finishedTasks.isEmpty()) {
        auto *sep = new QFrame(this);
        sep->setFrameShape(QFrame::HLine);
        sep->setStyleSheet("color: #d0d0d0;");
        sep->setFixedHeight(1);
        m_layout->addWidget(sep);
    }

    for (const auto &info : finishedTasks)
        m_layout->addWidget(_createTaskRow(info));

    const int totalRows = activeTasks.size() + finishedTasks.size();
    const bool hasSep = !activeTasks.isEmpty() && !finishedTasks.isEmpty();
    const int contentH = totalRows * kRowHeight + (hasSep ? 1 : 0);
    setFixedHeight(contentH + m_layout->contentsMargins().top()
                   + m_layout->contentsMargins().bottom());
}

void TaskHistoryPopup::showAbove(
    QWidget *anchor, const QList<ProgressManager::TaskInfo> &activeTasks,
    const QList<ProgressManager::TaskInfo> &finishedTasks)
{
    if (!anchor)
        return;

    refresh(activeTasks, finishedTasks);

    const QPoint btnGlobal = anchor->mapToGlobal(QPoint(0, 0));
    const int btnWidth = anchor->width();
    const int popupW = width();

    int x = btnGlobal.x() + (btnWidth - popupW) / 2;
    int y = btnGlobal.y() - height() - 2;

    if (auto *screen = QApplication::screenAt(btnGlobal)) {
        const QRect scr = screen->availableGeometry();
        if (x + popupW > scr.right())
            x = scr.right() - popupW - 4;
        if (x < scr.left())
            x = scr.left() + 4;
        if (y < scr.top())
            y = btnGlobal.y() + anchor->height() + 2;
    }

    move(x, y);
    show();
    setFocus();
}

void TaskHistoryPopup::focusOutEvent(QFocusEvent *event)
{
    QFrame::focusOutEvent(event);
    hide();
    emit popupHidden();
}

bool TaskHistoryPopup::eventFilter(QObject *obj, QEvent *event)
{
    auto *row = qobject_cast<QFrame *>(obj);
    if (!row)
        return QFrame::eventFilter(obj, event);

    const QVariant taskIdVar = row->property("taskId");
    if (!taskIdVar.isValid())
        return QFrame::eventFilter(obj, event);

    const QString taskId = taskIdVar.toString();

    if (event->type() == QEvent::Enter) {
        row->setStyleSheet(QStringLiteral(
            "background-color: #e0e8f0; border: none; border-radius: 2px;"));
    } else if (event->type() == QEvent::Leave) {
        row->setStyleSheet(
            QStringLiteral("background-color: transparent; border: none;"));
    } else if (event->type() == QEvent::MouseButtonRelease) {
        hide();
        emit popupHidden();
        emit taskClicked(taskId);
    }

    return QFrame::eventFilter(obj, event);
}

void TaskHistoryPopup::_clearLayout()
{
    QLayoutItem *item;
    while ((item = m_layout->takeAt(0)) != nullptr) {
        if (item->widget())
            item->widget()->deleteLater();
        delete item;
    }
}

QWidget *TaskHistoryPopup::_createTaskRow(const ProgressManager::TaskInfo &info)
{
    auto *row = new QFrame(this);
    row->setFixedHeight(kRowHeight);

    auto *layout = new QHBoxLayout(row);
    layout->setContentsMargins(8, 0, 8, 0);
    layout->setSpacing(6);

    // 状态图标
    auto *iconLabel = new QLabel(row);
    iconLabel->setFixedSize(kIconSize, kIconSize);
    if (info.finished) {
        iconLabel->setText(QStringLiteral("✓")); // ✓
        iconLabel->setStyleSheet(QStringLiteral(
            "color: #4caf50; font-weight: bold; font-size: 12px;"));
    } else {
        iconLabel->setText(QStringLiteral("◌")); // ◌
        iconLabel->setStyleSheet(QStringLiteral(
            "color: #2196f3; font-weight: bold; font-size: 14px;"));
    }

    // 任务名
    auto *nameLabel = new QLabel(info.name, row);
    QFont f = nameLabel->font();
    f.setPointSize(9);
    nameLabel->setFont(f);
    if (info.finished) {
        nameLabel->setStyleSheet(
            QStringLiteral("color: #888888; border: none; padding: 0;"));
    } else {
        nameLabel->setStyleSheet(QStringLiteral("border: none; padding: 0;"));
    }

    // 进度文字
    auto *pctLabel = new QLabel(row);
    if (info.finished) {
        pctLabel->setText(QStringLiteral("100%"));
        pctLabel->setStyleSheet(
            QStringLiteral("color: #888888; border: none; padding: 0;"));
    } else {
        const int pct = info.max > 0 ? (info.value * 100 / info.max) : 0;
        pctLabel->setText(QStringLiteral("%1%").arg(pct));
        pctLabel->setStyleSheet(QStringLiteral(
            "color: #333333; font-weight: bold; border: none; padding: 0;"));
    }
    pctLabel->setFont(f);
    pctLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    pctLabel->setMinimumWidth(40);

    layout->addWidget(iconLabel);
    layout->addWidget(nameLabel, 1);
    layout->addWidget(pctLabel);

    // 仅进行中任务可点击
    if (!info.finished) {
        row->setCursor(Qt::PointingHandCursor);
        row->setProperty("taskId", info.id);
        row->installEventFilter(this);
    }

    return row;
}
