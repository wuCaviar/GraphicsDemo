// Copyright (C) 2016 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#ifndef QTGRADIENTSTOPSCONTROLLER_H
#define QTGRADIENTSTOPSCONTROLLER_H

#include <QtWidgets/QWidget>
#include <QMap>
#include "Common/ColorTypes.h"
#include "qtgradienteditor_global.h"

QT_BEGIN_NAMESPACE

namespace Ui {
    class QtGradientEditor;
}

class QTGRADIENTEDITOR_EXPORT QtGradientStopsController : public QObject
{
    Q_OBJECT
public:
    QtGradientStopsController(QObject *parent = 0);
    ~QtGradientStopsController();

    void setUi(Ui::QtGradientEditor *editor);

    void setGradientStops(const QGradientStops &stops);
    QGradientStops gradientStops() const;

    // Per-stop CMYK storage for accurate TIFF export
    void setStopsCmyk(const QMap<qreal, CmykColor> &cmykMap);
    QMap<qreal, CmykColor> stopsCmyk() const;

signals:
    void gradientStopsChanged(const QGradientStops &stops);

private:
    QScopedPointer<class QtGradientStopsControllerPrivate> d_ptr;
    Q_DECLARE_PRIVATE(QtGradientStopsController)
    Q_DISABLE_COPY_MOVE(QtGradientStopsController)
};

QT_END_NAMESPACE

#endif
