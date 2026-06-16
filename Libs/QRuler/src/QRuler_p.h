/* This file is part of the KDE project
 * SPDX-FileCopyrightText: 2007 Thomas Zander <zander@kde.org>
 *
 * SPDX-License-Identifier: LGPL-2.0-or-later
 *
 * Extracted as a standalone widget from the Krita codebase.
 */
#ifndef QRULER_P_H
#define QRULER_P_H

#include "Unit.h"

class RulerTabChooser : public QWidget
{
public:
    RulerTabChooser(QWidget *parent)
        : QWidget(parent)
        , m_type(QTextOption::LeftTab)
        , m_showTabs(false)
    {
    }
    ~RulerTabChooser() override {}

    inline QTextOption::TabType type() { return m_type; }
    void setShowTabs(bool showTabs)
    {
        if (m_showTabs == showTabs)
            return;
        m_showTabs = showTabs;
        update();
    }
    void mousePressEvent(QMouseEvent *) override;
    void paintEvent(QPaintEvent *) override;

private:
    QTextOption::TabType m_type { QTextOption::LeftTab };
    bool m_showTabs : 1;
};

class PaintingStrategy
{
public:
    PaintingStrategy() {}
    virtual ~PaintingStrategy() {}

    virtual QRectF drawBackground(const class QRulerPrivate *ruler, QPainter &painter) = 0;
    virtual void drawTabs(const class QRulerPrivate *ruler, QPainter &painter) = 0;
    virtual void drawMeasurements(const class QRulerPrivate *ruler, QPainter &painter, const QRectF &rectangle) = 0;
    virtual void drawIndents(const class QRulerPrivate *ruler, QPainter &painter) = 0;
    virtual QSize sizeHint() = 0;
};

class HorizontalPaintingStrategy : public PaintingStrategy
{
public:
    HorizontalPaintingStrategy()
        : lengthInPixel(1)
    {
    }

    QRectF drawBackground(const QRulerPrivate *ruler, QPainter &painter) override;
    void drawTabs(const QRulerPrivate *ruler, QPainter &painter) override;
    void drawMeasurements(const QRulerPrivate *ruler, QPainter &painter, const QRectF &rectangle) override;
    void drawIndents(const QRulerPrivate *ruler, QPainter &painter) override;
    QSize sizeHint() override;

private:
    qreal lengthInPixel { 0.0 };
};

class VerticalPaintingStrategy : public PaintingStrategy
{
public:
    VerticalPaintingStrategy()
        : lengthInPixel(1)
    {
    }

    QRectF drawBackground(const QRulerPrivate *ruler, QPainter &painter) override;
    void drawTabs(const QRulerPrivate *, QPainter &) override {}
    void drawMeasurements(const QRulerPrivate *ruler, QPainter &painter, const QRectF &rectangle) override;
    void drawIndents(const QRulerPrivate *, QPainter &) override {}
    QSize sizeHint() override;

private:
    qreal lengthInPixel { 0.0 };
};

class HorizontalDistancesPaintingStrategy : public HorizontalPaintingStrategy
{
public:
    HorizontalDistancesPaintingStrategy() {}

    void drawMeasurements(const QRulerPrivate *ruler, QPainter &painter, const QRectF &rectangle) override;

private:
    void drawDistanceLine(const QRulerPrivate *d, QPainter &painter, const qreal start, const qreal end);
};

class QRulerPrivate
{
public:
    QRulerPrivate(QRuler *parent, const ViewConverter *vc, Qt::Orientation orientation);
    ~QRulerPrivate();

    void emitTabChanged();

    Unit unit;
    const Qt::Orientation orientation;
    const ViewConverter *const viewConverter;

    int offset { 0 };
    qreal rulerLength { 0.0 };
    qreal activeRangeStart { 0.0 };
    qreal activeRangeEnd { 0.0 };
    qreal activeOverrideRangeStart { 0.0 };
    qreal activeOverrideRangeEnd { 0.0 };

    int mouseCoordinate { 0 };
    int showMousePosition { 0 };

    bool showSelectionBorders { false };
    qreal firstSelectionBorder { 0.0 };
    qreal secondSelectionBorder { 0.0 };

    bool showIndents { false };
    qreal firstLineIndent { 0.0 };
    qreal paragraphIndent { 0.0 };
    qreal endIndent { 0.0 };

    bool showTabs { false };
    bool relativeTabs { false };
    bool tabMoved { false };
    QList<QRuler::Tab> tabs;
    int originalIndex { 0 };
    int currentIndex { 0 };
    QRuler::Tab deletedTab;
    qreal tabDistance { 0.0 };

    struct HotSpotData {
        qreal position;
        int id;
    };
    QList<HotSpotData> hotspots;

    bool rightToLeft { false };
    enum Selection {
        None,
        Tab,
        FirstLineIndent,
        ParagraphIndent,
        EndIndent,
        HotSpot
    };
    Selection selected { None };
    int selectOffset { 0 };

    QList<QAction *> popupActions;

    RulerTabChooser *tabChooser { nullptr };

    // Cached painting strategies
    PaintingStrategy *normalPaintingStrategy { nullptr };
    PaintingStrategy *distancesPaintingStrategy { nullptr };

    // Current painting strategy
    PaintingStrategy *paintingStrategy { nullptr };

    QRuler *ruler { nullptr };

    bool guideCreationStarted { false };

    qreal pixelStep { 0.0 };

    qreal numberStepForUnit() const;
    /// @return The rounding of value to the nearest multiple of stepValue
    qreal doSnapping(const qreal value) const;
    Selection selectionAtPosition(const QPoint &pos, int *selectOffset = nullptr);
    int hotSpotIndex(const QPoint &pos);
    qreal effectiveActiveRangeStart() const;
    qreal effectiveActiveRangeEnd() const;

    friend class VerticalPaintingStrategy;
    friend class HorizontalPaintingStrategy;
};

#endif // QRULER_P_H
