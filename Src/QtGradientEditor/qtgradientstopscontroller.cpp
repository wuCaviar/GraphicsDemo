// Copyright (C) 2016 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "qtgradientstopscontroller.h"
#include "ui_qtgradienteditor.h"
#include "qtgradientstopsmodel.h"

#include <QtCore/QTimer>

QT_BEGIN_NAMESPACE

using namespace Qt::StringLiterals;

class QtGradientStopsControllerPrivate : public QObject
{
    Q_OBJECT
    QtGradientStopsController *q_ptr;
    Q_DECLARE_PUBLIC(QtGradientStopsController)
public:
    using PositionColorMap = QMap<qreal, QColor>;
    using PositionStopMap = QMap<qreal, QtGradientStop *>;

    void setUi(Ui::QtGradientEditor *ui);

    void slotHsvClicked();
    void slotRgbClicked();

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

    QtGradientStopsModel *m_model = nullptr;
    QColor::Spec m_spec = QColor::Hsv;

    Ui::QtGradientEditor *m_ui;
};

void QtGradientStopsControllerPrivate::setUi(Ui::QtGradientEditor *ui)
{
    m_ui = ui;

    m_model = new QtGradientStopsModel(this);
    m_ui->gradientStopsWidget->setGradientStopsModel(m_model);
    connect(m_model, &QtGradientStopsModel::currentStopChanged,
            this, &QtGradientStopsControllerPrivate::slotCurrentStopChanged);
    connect(m_model, &QtGradientStopsModel::stopMoved,
            this, &QtGradientStopsControllerPrivate::slotStopMoved);
    connect(m_model, &QtGradientStopsModel::stopsSwapped,
            this, &QtGradientStopsControllerPrivate::slotStopsSwapped);
    connect(m_model, &QtGradientStopsModel::stopChanged,
            this, &QtGradientStopsControllerPrivate::slotStopChanged);
    connect(m_model, &QtGradientStopsModel::stopSelected,
            this, &QtGradientStopsControllerPrivate::slotStopSelected);
    connect(m_model, &QtGradientStopsModel::stopAdded,
            this, &QtGradientStopsControllerPrivate::slotStopAdded);
    connect(m_model, &QtGradientStopsModel::stopRemoved,
            this, &QtGradientStopsControllerPrivate::slotStopRemoved);

    connect(m_ui->colorWidget, &color_widgets::ColorSelector::colorChanged,
            this, &QtGradientStopsControllerPrivate::slotChangeColor);

    connect(m_ui->positionSpinBox, &QDoubleSpinBox::valueChanged,
            this, &QtGradientStopsControllerPrivate::slotChangePosition);

    connect(m_ui->zoomSpinBox, &QSpinBox::valueChanged,
            this, &QtGradientStopsControllerPrivate::slotChangeZoom);
    connect(m_ui->zoomInButton, &QToolButton::clicked,
            this, &QtGradientStopsControllerPrivate::slotZoomIn);
    connect(m_ui->zoomOutButton, &QToolButton::clicked,
            this, &QtGradientStopsControllerPrivate::slotZoomOut);
    connect(m_ui->zoomAllButton, &QToolButton::clicked,
            this, &QtGradientStopsControllerPrivate::slotZoomAll);
    connect(m_ui->gradientStopsWidget, &QtGradientStopsWidget::zoomChanged,
            this, &QtGradientStopsControllerPrivate::slotZoomChanged);

    enableCurrent(false);
    m_ui->zoomInButton->setIcon(QIcon(":/qtgradienteditor/images/zoomin.png"_L1));
    m_ui->zoomOutButton->setIcon(QIcon(":/qtgradienteditor/images/zoomout.png"_L1));
    updateZoom(1);
}

void QtGradientStopsControllerPrivate::enableCurrent(bool enable)
{
    m_ui->positionLabel->setEnabled(enable);
    m_ui->colorLabel->setEnabled(enable);

    m_ui->positionSpinBox->setEnabled(enable);
    m_ui->colorWidget->setEnabled(enable);
}

QtGradientStopsControllerPrivate::PositionColorMap QtGradientStopsControllerPrivate::stopsData(const PositionStopMap &stops) const
{
    PositionColorMap data;
    for (QtGradientStop *stop : stops)
        data[stop->position()] = stop->color();
    return data;
}

QGradientStops QtGradientStopsControllerPrivate::makeGradientStops(const PositionColorMap &data) const
{
    QGradientStops stops;
    for (auto itData = data.cbegin(), cend = data.cend(); itData != cend; ++itData)
        stops << std::pair<qreal, QColor>(itData.key(), itData.value());
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

    QTimer::singleShot(0, this, &QtGradientStopsControllerPrivate::slotUpdatePositionSpinBox);
}

void QtGradientStopsControllerPrivate::slotStopMoved(QtGradientStop *stop, qreal newPos)
{
    QTimer::singleShot(0, this, &QtGradientStopsControllerPrivate::slotUpdatePositionSpinBox);

    PositionColorMap stops = stopsData(m_model->stops());
    stops.remove(stop->position());
    stops[newPos] = stop->color();

    QGradientStops gradStops = makeGradientStops(stops);
    emit q_ptr->gradientStopsChanged(gradStops);
}

void QtGradientStopsControllerPrivate::slotStopsSwapped(QtGradientStop *stop1, QtGradientStop *stop2)
{
    QTimer::singleShot(0, this, &QtGradientStopsControllerPrivate::slotUpdatePositionSpinBox);

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
    Q_UNUSED(stop);
    Q_UNUSED(selected);
    QTimer::singleShot(0, this, &QtGradientStopsControllerPrivate::slotUpdatePositionSpinBox);
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
        m_ui->positionSpinBox->setRange(double(newMin) / 1000, double(newMax) / 1000);
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
    const auto stops = m_model->selectedStops();
    for (QtGradientStop *s : stops) {
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
    d_ptr->setUi(ui);
}

QtGradientStopsController::~QtGradientStopsController()
{
}

void QtGradientStopsController::setGradientStops(const QGradientStops &stops)
{
    d_ptr->m_model->clear();
    QtGradientStop *first = nullptr;
    for (const std::pair<qreal, QColor> &pair : stops) {
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
    const auto stopsList = d_ptr->m_model->stops().values();
    for (const QtGradientStop *stop : stopsList)
        stops.append({stop->position(), stop->color()});
    return stops;
}

QT_END_NAMESPACE

#include "qtgradientstopscontroller.moc"
