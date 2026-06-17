#ifndef QATCURSOR_H
#define QATCURSOR_H

#include <QApplication>

class QAtCursor
{
public:
    QAtCursor() { QApplication::setOverrideCursor(Qt::WaitCursor); }

    ~QAtCursor() { QApplication::restoreOverrideCursor(); }
};

#endif // QATCURSOR_H