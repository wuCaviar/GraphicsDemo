/****************************************************************************
**
** Copyright (C) 2009 Nokia Corporation and/or its subsidiary(-ies).
** All rights reserved.
** Contact: Nokia Corporation (qt-info@nokia.com)
**
** This file is part of the tools applications of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL$
** No Commercial Usage
** This file contains pre-release code and may not be distributed.
** You may use this file in accordance with the terms and conditions
** contained in the Technology Preview License Agreement accompanying
** this package.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU Lesser General Public License version 2.1 requirements
** will be met: http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Nokia gives you certain additional
** rights.  These rights are described in the Nokia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** If you have questions regarding the use of this file, please contact
** Nokia at qt-info@nokia.com.
**
**
**
**
**
**
**
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "qtgradientstopscontroller.h"
#include "ui_qtgradienteditor.h"
#include "qtgradientstopsmodel.h"

#include <QtCore/QTimer>
#include <QVector>

QT_BEGIN_NAMESPACE

class QtGradientStopsControllerPrivate
{
    QtGradientStopsController *q_ptr;
    Q_DECLARE_PUBLIC(QtGradientStopsController)
public:
    typedef QMap<qreal, QColor> PositionColorMap;
    typedef QMap<qreal, QtGradientStop *> PositionStopMap;

    void slotCurrentStopChanged(QtGradientStop *stop);
    void slotStopMoved(QtGradientStop *stop, qreal newPos);
    void slotStopsSwapped(QtGradientStop *stop1, QtGradientStop *stop2);
    void slotStopChanged(QtGradientStop *stop, const QColor &newColor);
    void slotStopSelected(QtGradientStop *stop, bool selected);
    void slotStopAdded(QtGradientStop *stop);
    void slotStopRemoved(QtGradientStop *stop);
    void slotUpdatePositionSpinBox();

    void slotChangeColor(const QColor &color);
    void slotChangePosition(double value);

    void slotChangeZoom(int value);
    void slotZoomIn();
    void slotZoomOut();
    void slotZoomAll();
    void slotZoomChanged(double zoom);

    void enableCurrent(bool enable);
    PositionColorMap stopsData(const PositionStopMap &stops) const;
    QGradientStops makeGradientStops(const PositionColorMap &data) const;
    void updateZoom(double zoom);

    QtGradientStopsModel *m_model;

    Ui::QtGradientEditor *m_ui;
};

void QtGradientStopsControllerPrivate::enableCurrent(bool enable)
{
    m_ui->positionLabel->setEnabled(enable);
    m_ui->colorLabel->setEnabled(enable);
    m_ui->colorWidget->setEnabled(enable);
    m_ui->positionSpinBox->setEnabled(enable);
}

QtGradientStopsControllerPrivate::PositionColorMap QtGradientStopsControllerPrivate::stopsData(const PositionStopMap &stops) const
{
    PositionColorMap data;
    PositionStopMap::ConstIterator itStop = stops.constBegin();
    while (itStop != stops.constEnd()) {
        QtGradientStop *stop = itStop.value();
        data[stop->position()] = stop->color();

        ++itStop;
    }
    return data;
}

QGradientStops QtGradientStopsControllerPrivate::makeGradientStops(const PositionColorMap &data) const
{
    QGradientStops stops;
    PositionColorMap::ConstIterator itData = data.constBegin();
    while (itData != data.constEnd()) {
        stops << QPair<qreal, QColor>(itData.key(), itData.value());

        ++itData;
    }
    return stops;
}

void QtGradientStopsControllerPrivate::updateZoom(double zoom)
{
    m_ui->gradientStopsWidget->setZoom(zoom);
    m_ui->zoomSpinBox->blockSignals(true);
    m_ui->zoomSpinBox->setValue(qRound(zoom * 100));
    m_ui->zoomSpinBox->blockSignals(false);
    bool zoomInEnabled = true;
    bool zoomOutEnabled = true;
    bool zoomAllEnabled = true;
    if (zoom <= 1) {
        zoomAllEnabled = false;
        zoomOutEnabled = false;
    } else if (zoom >= 100) {
        zoomInEnabled = false;
    }
    m_ui->zoomInButton->setEnabled(zoomInEnabled);
    m_ui->zoomOutButton->setEnabled(zoomOutEnabled);
    m_ui->zoomAllButton->setEnabled(zoomAllEnabled);
}

void QtGradientStopsControllerPrivate::slotCurrentStopChanged(QtGradientStop *stop)
{
    if (!stop) {
        enableCurrent(false);
        return;
    }
    enableCurrent(true);

    m_ui->colorWidget->setColor(stop->color());

    QTimer::singleShot(0, q_ptr, SLOT(slotUpdatePositionSpinBox()));
}

void QtGradientStopsControllerPrivate::slotStopMoved(QtGradientStop *stop, qreal newPos)
{
    QTimer::singleShot(0, q_ptr, SLOT(slotUpdatePositionSpinBox()));

    PositionColorMap stops = stopsData(m_model->stops());
    stops.remove(stop->position());
    stops[newPos] = stop->color();

    QGradientStops gradStops = makeGradientStops(stops);
    emit q_ptr->gradientStopsChanged(gradStops);
}

void QtGradientStopsControllerPrivate::slotStopsSwapped(QtGradientStop *stop1, QtGradientStop *stop2)
{
    QTimer::singleShot(0, q_ptr, SLOT(slotUpdatePositionSpinBox()));

    PositionColorMap stops = stopsData(m_model->stops());
    const qreal pos1 = stop1->position();
    const qreal pos2 = stop2->position();
    stops[pos1] = stop2->color();
    stops[pos2] = stop1->color();

    QGradientStops gradStops = makeGradientStops(stops);
    emit q_ptr->gradientStopsChanged(gradStops);
}

void QtGradientStopsControllerPrivate::slotStopAdded(QtGradientStop *stop)
{
    PositionColorMap stops = stopsData(m_model->stops());
    stops[stop->position()] = stop->color();

    QGradientStops gradStops = makeGradientStops(stops);
    emit q_ptr->gradientStopsChanged(gradStops);
}

void QtGradientStopsControllerPrivate::slotStopRemoved(QtGradientStop *stop)
{
    PositionColorMap stops = stopsData(m_model->stops());
    stops.remove(stop->position());

    QGradientStops gradStops = makeGradientStops(stops);
    emit q_ptr->gradientStopsChanged(gradStops);
}

void QtGradientStopsControllerPrivate::slotStopChanged(QtGradientStop *stop, const QColor &newColor)
{
    if (m_model->currentStop() == stop) {
        m_ui->colorWidget->setColor(newColor);
    }

    PositionColorMap stops = stopsData(m_model->stops());
    stops[stop->position()] = newColor;

    QGradientStops gradStops = makeGradientStops(stops);
    emit q_ptr->gradientStopsChanged(gradStops);
}

void QtGradientStopsControllerPrivate::slotStopSelected(QtGradientStop *stop, bool selected)
{
    Q_UNUSED(stop)
    Q_UNUSED(selected)
    QTimer::singleShot(0, q_ptr, SLOT(slotUpdatePositionSpinBox()));
}

void QtGradientStopsControllerPrivate::slotUpdatePositionSpinBox()
{
    QtGradientStop *current = m_model->currentStop();
    if (!current)
        return;

    qreal min = 0.0;
    qreal max = 1.0;
    const qreal pos = current->position();

    QtGradientStop *first = m_model->firstSelected();
    QtGradientStop *last = m_model->lastSelected();

    if (first && last) {
        const qreal minPos = pos - first->position() - 0.0004999;
        const qreal maxPos = pos + 1.0 - last->position() + 0.0004999;

        if (max > maxPos)
            max = maxPos;
        if (min < minPos)
            min = minPos;

        if (first->position() == 0.0)
            min = pos;
        if (last->position() == 1.0)
            max = pos;
    }

    const int spinMin = qRound(m_ui->positionSpinBox->minimum() * 1000);
    const int spinMax = qRound(m_ui->positionSpinBox->maximum() * 1000);

    const int newMin = qRound(min * 1000);
    const int newMax = qRound(max * 1000);

    m_ui->positionSpinBox->blockSignals(true);
    if (spinMin != newMin || spinMax != newMax) {
        m_ui->positionSpinBox->setRange((double)newMin / 1000, (double)newMax / 1000);
    }
    if (m_ui->positionSpinBox->value() != pos)
        m_ui->positionSpinBox->setValue(pos);
    m_ui->positionSpinBox->blockSignals(false);
}

void QtGradientStopsControllerPrivate::slotChangeColor(const QColor &color)
{
    QtGradientStop *stop = m_model->currentStop();
    if (!stop)
        return;
    m_model->changeStop(stop, color);
    QList<QtGradientStop *> stops = m_model->selectedStops();
    QListIterator<QtGradientStop *> itStop(stops);
    while (itStop.hasNext()) {
        QtGradientStop *s = itStop.next();
        if (s != stop)
            m_model->changeStop(s, color);
    }
}

void QtGradientStopsControllerPrivate::slotChangePosition(double value)
{
    QtGradientStop *stop = m_model->currentStop();
    if (!stop)
        return;

    m_model->moveStops(value);
}

void QtGradientStopsControllerPrivate::slotChangeZoom(int value)
{
    updateZoom(value / 100.0);
}

void QtGradientStopsControllerPrivate::slotZoomIn()
{
    double newZoom = m_ui->gradientStopsWidget->zoom() * 2;
    if (newZoom > 100)
        newZoom = 100;
    updateZoom(newZoom);
}

void QtGradientStopsControllerPrivate::slotZoomOut()
{
    double newZoom = m_ui->gradientStopsWidget->zoom() / 2;
    if (newZoom < 1)
        newZoom = 1;
    updateZoom(newZoom);
}

void QtGradientStopsControllerPrivate::slotZoomAll()
{
    updateZoom(1);
}

void QtGradientStopsControllerPrivate::slotZoomChanged(double zoom)
{
    updateZoom(zoom);
}

QtGradientStopsController::QtGradientStopsController(QObject *parent)
    : QObject(parent), d_ptr(new QtGradientStopsControllerPrivate())
{
    d_ptr->q_ptr = this;
}

void QtGradientStopsController::setUi(Ui::QtGradientEditor *ui)
{
    d_ptr->m_ui = ui;

    d_ptr->m_model = new QtGradientStopsModel(this);
    d_ptr->m_ui->gradientStopsWidget->setGradientStopsModel(d_ptr->m_model);
    connect(d_ptr->m_model, SIGNAL(currentStopChanged(QtGradientStop*)),
                this, SLOT(slotCurrentStopChanged(QtGradientStop*)));
    connect(d_ptr->m_model, SIGNAL(stopMoved(QtGradientStop*,qreal)),
                this, SLOT(slotStopMoved(QtGradientStop*,qreal)));
    connect(d_ptr->m_model, SIGNAL(stopsSwapped(QtGradientStop*,QtGradientStop*)),
                this, SLOT(slotStopsSwapped(QtGradientStop*,QtGradientStop*)));
    connect(d_ptr->m_model, SIGNAL(stopChanged(QtGradientStop*,QColor)),
                this, SLOT(slotStopChanged(QtGradientStop*,QColor)));
    connect(d_ptr->m_model, SIGNAL(stopSelected(QtGradientStop*,bool)),
                this, SLOT(slotStopSelected(QtGradientStop*,bool)));
    connect(d_ptr->m_model, SIGNAL(stopAdded(QtGradientStop*)),
                this, SLOT(slotStopAdded(QtGradientStop*)));
    connect(d_ptr->m_model, SIGNAL(stopRemoved(QtGradientStop*)),
                this, SLOT(slotStopRemoved(QtGradientStop*)));

    connect(d_ptr->m_ui->colorWidget, SIGNAL(colorSelected(QColor)),
                this, SLOT(slotChangeColor(QColor)));
    connect(d_ptr->m_ui->positionSpinBox, SIGNAL(valueChanged(double)),
                this, SLOT(slotChangePosition(double)));

    connect(d_ptr->m_ui->zoomSpinBox, SIGNAL(valueChanged(int)),
                this, SLOT(slotChangeZoom(int)));
    connect(d_ptr->m_ui->zoomInButton, SIGNAL(clicked()),
                this, SLOT(slotZoomIn()));
    connect(d_ptr->m_ui->zoomOutButton, SIGNAL(clicked()),
                this, SLOT(slotZoomOut()));
    connect(d_ptr->m_ui->zoomAllButton, SIGNAL(clicked()),
                this, SLOT(slotZoomAll()));
    connect(d_ptr->m_ui->gradientStopsWidget, SIGNAL(zoomChanged(double)),
                this, SLOT(slotZoomChanged(double)));

    d_ptr->enableCurrent(false);
    d_ptr->m_ui->zoomInButton->setIcon(QIcon(QLatin1String(":/trolltech/qtgradienteditor/images/zoomin.png")));
    d_ptr->m_ui->zoomOutButton->setIcon(QIcon(QLatin1String(":/trolltech/qtgradienteditor/images/zoomout.png")));
    d_ptr->updateZoom(1);
}

QtGradientStopsController::~QtGradientStopsController()
{
}

void QtGradientStopsController::setGradientStops(const QGradientStops &stops)
{
    d_ptr->m_model->clear();
    QVectorIterator<QPair<qreal, QColor> > it(stops);
    QtGradientStop *first = 0;
    while (it.hasNext()) {
        QPair<qreal, QColor> pair = it.next();
        QtGradientStop *stop = d_ptr->m_model->addStop(pair.first, pair.second);
        if (!first)
            first = stop;
    }
    if (first)
        d_ptr->m_model->setCurrentStop(first);
}

QGradientStops QtGradientStopsController::gradientStops() const
{
    QGradientStops stops;
    QList<QtGradientStop *> stopsList = d_ptr->m_model->stops().values();
    QListIterator<QtGradientStop *> itStop(stopsList);
    while (itStop.hasNext()) {
        QtGradientStop *stop = itStop.next();
        stops << QPair<qreal, QColor>(stop->position(), stop->color());
    }
    return stops;
}

QT_END_NAMESPACE

#include "moc_qtgradientstopscontroller.cpp"
