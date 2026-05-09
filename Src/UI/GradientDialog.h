#ifndef GRADIENTDIALOG_H
#define GRADIENTDIALOG_H

#include <QWidget>

class GradientPreview : public QWidget
{
    Q_OBJECT
    Q_PROPERTY(QBrush brush READ brush WRITE setBrush NOTIFY brushChanged DESIGNABLE true)
    Q_PROPERTY(QBrush background READ background WRITE setBackground NOTIFY backgroundChanged DESIGNABLE true)
    Q_PROPERTY(bool drawFrame READ drawFrame WRITE setDrawFrame NOTIFY drawFrameChanged DESIGNABLE true)

public:
    explicit GradientPreview(QWidget *parent = nullptr);
    ~GradientPreview();

           /// Get the background visible under transparent colors
    QBrush background() const;

           /// Change the background visible under transparent colors
    void setBackground(const QBrush &bk);

           /// Get current brush
    QBrush brush() const;

    QSize sizeHint () const Q_DECL_OVERRIDE;

    void paint(QPainter &painter, QRect rect) const;

           /// Whether to draw a frame around the color
    bool drawFrame() const;
    void setDrawFrame(bool);

public Q_SLOTS:
    /// Set current color
    void setBrush(const QBrush &b);

Q_SIGNALS:
    /// Emitted when the user clicks on the widget
    void clicked();

           /// Emitted on setColor
    void brushChanged(QBrush);

    void backgroundChanged(const QBrush&);
    void drawFrameChanged(bool);

protected:
    void paintEvent(QPaintEvent *) Q_DECL_OVERRIDE;
    void resizeEvent(QResizeEvent *) Q_DECL_OVERRIDE;
    void mouseReleaseEvent(QMouseEvent *ev) Q_DECL_OVERRIDE;
    void mouseMoveEvent(QMouseEvent *ev) Q_DECL_OVERRIDE;

private:
    class Private;
    Private * const p;
};

#endif // GRADIENTDIALOG_H
