#include "GradientDialog.h"

#include "ColorUtils.h"

#include <qtgradientdialog.h>

#include <QDrag>
#include <QLinearGradient>
#include <QMimeData>
#include <QMouseEvent>
#include <QStyleOptionFrame>
#include <QStylePainter>

class GradientPreview::Private
{
public:
    QBrush brush;     ///< 渐变画刷
    QBrush back;      ///< 透明区域背景（棋盘格）
    bool draw_frame = true; ///< 是否绘制边框

    Private()
        : brush(QGradient(QGradient::WarmFlame)),
          back(Qt::darkGray, Qt::DiagCrossPattern)
    {}
};

GradientPreview::GradientPreview(QWidget *parent)
    : QWidget(parent), p(new Private)
{
    p->back.setTexture(QPixmap(QStringLiteral(":/color_widgets/alphaback.png")));
    setMinimumSize(sizeHint());
}

GradientPreview::~GradientPreview()
{
    delete p;
}

void GradientPreview::setBackground(const QBrush &bk)
{
    p->back = bk;
    update();
    Q_EMIT backgroundChanged(bk);
}

QBrush GradientPreview::background() const
{
    return p->back;
}

QBrush GradientPreview::brush() const
{
    return p->brush;
}

QSize GradientPreview::sizeHint() const
{
    int width = style()->pixelMetric(QStyle::PM_IndicatorWidth, nullptr, nullptr);
    int height = style()->pixelMetric(QStyle::PM_IndicatorHeight, nullptr, nullptr);
    return QSize(qMax(72, width), qMax(24, height));
}

void GradientPreview::paint(QPainter &painter, QRect rect) const
{
    // 绘制凹陷边框
    if (p->draw_frame) {
        QStyleOptionFrame panel;
        panel.initFrom(this);
        panel.lineWidth = 2;
        panel.midLineWidth = 0;
        panel.state |= QStyle::State_Sunken;
        style()->drawPrimitive(QStyle::PE_Frame, &panel, &painter, this);
        QRect r = style()->subElementRect(QStyle::SE_FrameContents, &panel, this);
        painter.setClipRect(r);
    }

    const QGradient *grad = p->brush.gradient();

    // 检测是否含有透明色，若有则先绘制棋盘格背景
    bool hasAlpha = false;
    if (grad) {
        const auto stops = grad->stops();
        for (const QGradientStop &stop : stops) {
            if (stop.second.alpha() < 255) {
                hasAlpha = true;
                break;
            }
        }
    } else if (p->brush.color().alpha() < 255) {
        hasAlpha = true;
    }
    if (hasAlpha)
        painter.fillRect(0, 0, rect.width(), rect.height(), p->back);

    // 若无渐变，直接填充画刷
    if (!grad) {
        painter.fillRect(0, 0, rect.width(), rect.height(), p->brush);
        return;
    }

    QRectF previewRect(0, 0, rect.width(), rect.height());
    painter.fillRect(previewRect, ColorUtils::mapGradientBrushToRect(p->brush, previewRect));
}

void GradientPreview::setBrush(const QBrush &c)
{
    p->brush = c;
    update();
    Q_EMIT brushChanged(c);
}

void GradientPreview::paintEvent(QPaintEvent *)
{
    // 注意：必须使用 rect() 而非 geometry()
    // geometry() 返回相对于父控件的坐标，在自身坐标系下绘制会偏移
    QStylePainter painter(this);
    paint(painter, rect());
}

void GradientPreview::resizeEvent(QResizeEvent *)
{
    update();
}

void GradientPreview::mouseReleaseEvent(QMouseEvent *ev)
{
    if (!QRect(QPoint(0, 0), size()).contains(ev->pos()))
        return;

    Q_EMIT clicked();

    // 创建渐变编辑对话框
    QtGradientDialog dialog(this);
    dialog.setWindowTitle(tr("Edit Gradient"));

    // 将当前画刷中的渐变设置到对话框
    if (p->brush.gradient())
        dialog.setGradient(*p->brush.gradient());
    else
        dialog.setGradient(QLinearGradient(0, 0, 1, 0));

    // 连接 gradientChanged 信号实现实时预览
    connect(&dialog, &QtGradientDialog::gradientChanged, this,
            [this](const QGradient &gradient) {
                p->brush = QBrush(gradient);
                update();
                Q_EMIT brushChanged(p->brush);
            });

    dialog.exec();

    Q_EMIT brushChanged(p->brush);
}

void GradientPreview::mouseMoveEvent(QMouseEvent *ev)
{
    if (!(ev->buttons() & Qt::LeftButton))
        return;
    if (QRect(QPoint(0, 0), size()).contains(ev->pos()))
        return;

    QMimeData *data = new QMimeData;
    data->setColorData(p->brush);

    QDrag *drag = new QDrag(this);
    drag->setMimeData(data);

    // 生成拖拽预览图
    QPixmap pixmap;
    pixmap.fill(Qt::transparent);
    QPainter pPainter(&pixmap);
    pPainter.fillRect(0, 0, size().width(), size().height(), p->brush);
    pPainter.end();

    drag->setPixmap(pixmap);
    drag->exec(Qt::CopyAction);
}

bool GradientPreview::drawFrame() const
{
    return p->draw_frame;
}

void GradientPreview::setDrawFrame(bool draw)
{
    if (p->draw_frame == draw)
        return;
    p->draw_frame = draw;
    Q_EMIT drawFrameChanged(draw);
    update();
}
