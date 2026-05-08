#include "GradientDialog.h"

#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPainter>
#include <QPaintEvent>
#include <QPushButton>
#include <QVBoxLayout>

#include "ColorUtils.h"

// 渐变预览控件
class GradientPreviewWidget : public QWidget
{
public:
    explicit GradientPreviewWidget(QWidget *parent = nullptr) : QWidget(parent)
    {
        setFixedHeight(40);
        setMinimumWidth(200);
    }

    void setGradient(const QLinearGradient &g) { m_gradient = g; update(); }

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter p(this);
        // 先画棋盘格背景（表示透明）
        p.fillRect(rect(), Qt::white);
        for (int y = 0; y < height(); y += 8) {
            for (int x = 0; x < width(); x += 8) {
                if ((x / 8 + y / 8) % 2 == 0)
                    p.fillRect(x, y, 8, 8, QColor(220, 220, 220));
            }
        }
        // 画渐变
        QLinearGradient drawGradient = m_gradient;
        drawGradient.setStart(0, 0);
        drawGradient.setFinalStop(width(), 0);
        p.fillRect(rect(), QBrush(drawGradient));
        // 边框
        p.setPen(QPen(Qt::gray, 1));
        p.drawRect(rect().adjusted(0, 0, -1, -1));
    }

private:
    QLinearGradient m_gradient;
};

// 渐变方向枚举
enum GradientDirection {
    LeftToRight = 0,
    RightToLeft = 1,
    TopToBottom = 2,
    BottomToTop = 3,
    DiagonalTL = 4,
    DiagonalBR = 5
};

GradientDialog::GradientDialog(const QLinearGradient &gradient, QWidget *parent)
    : QDialog(parent)
{
    // 提取现有渐变的颜色
    QGradientStops stops = gradient.stops();
    if (stops.size() >= 2) {
        m_startColor = stops.first().second;
        m_endColor = stops.last().second;
    } else {
        m_startColor = Qt::white;
        m_endColor = Qt::black;
    }
    m_gradient = gradient;

    setupUI();
    updatePreview();
}

void GradientDialog::setupUI()
{
    setWindowTitle(tr("Gradient Editor"));
    setMinimumWidth(380);

    auto *mainLayout = new QVBoxLayout(this);

    // 颜色选择
    auto *colorGroup = new QGroupBox(tr("Colors"));
    auto *colorLayout = new QFormLayout(colorGroup);

    m_startColorBtn = new QPushButton;
    m_startColorBtn->setFixedSize(80, 28);
    m_startColorBtn->setStyleSheet(QString("background-color: %1").arg(m_startColor.name()));

    m_endColorBtn = new QPushButton;
    m_endColorBtn->setFixedSize(80, 28);
    m_endColorBtn->setStyleSheet(QString("background-color: %1").arg(m_endColor.name()));

    colorLayout->addRow(tr("Start Color:"), m_startColorBtn);
    colorLayout->addRow(tr("End Color:"), m_endColorBtn);
    mainLayout->addWidget(colorGroup);

    // 方向选择
    auto *dirGroup = new QGroupBox(tr("Direction"));
    auto *dirLayout = new QFormLayout(dirGroup);

    m_directionCombo = new QComboBox;
    m_directionCombo->addItems({
        tr("Left → Right"),
        tr("Right → Left"),
        tr("Top → Bottom"),
        tr("Bottom → Top"),
        tr("Diagonal ↘"),
        tr("Diagonal ↗"),
    });
    dirLayout->addRow(tr("Direction:"), m_directionCombo);
    mainLayout->addWidget(dirGroup);

    // 预览
    auto *previewGroup = new QGroupBox(tr("Preview"));
    auto *previewLayout = new QVBoxLayout(previewGroup);
    m_previewWidget = new GradientPreviewWidget;
    previewLayout->addWidget(m_previewWidget);
    mainLayout->addWidget(previewGroup);

    // 按钮
    auto *btnLayout = new QHBoxLayout;
    auto *okBtn = new QPushButton(tr("OK"));
    auto *cancelBtn = new QPushButton(tr("Cancel"));
    btnLayout->addStretch();
    btnLayout->addWidget(okBtn);
    btnLayout->addWidget(cancelBtn);
    mainLayout->addLayout(btnLayout);

    // 信号
    connect(m_startColorBtn, &QPushButton::clicked, this, &GradientDialog::onStartColorClicked);
    connect(m_endColorBtn, &QPushButton::clicked, this, &GradientDialog::onEndColorClicked);
    connect(m_directionCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            &GradientDialog::onDirectionChanged);
    connect(okBtn, &QPushButton::clicked, this, &QDialog::accept);
    connect(cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
}

void GradientDialog::onStartColorClicked()
{
    QColor c = ColorUtils::pickColor(this, m_startColor, tr("Start Color"));
    if (c.isValid()) {
        m_startColor = c;
        m_startColorBtn->setStyleSheet(QString("background-color: %1").arg(c.name()));
        updatePreview();
    }
}

void GradientDialog::onEndColorClicked()
{
    QColor c = ColorUtils::pickColor(this, m_endColor, tr("End Color"));
    if (c.isValid()) {
        m_endColor = c;
        m_endColorBtn->setStyleSheet(QString("background-color: %1").arg(c.name()));
        updatePreview();
    }
}

void GradientDialog::onDirectionChanged(int index)
{
    Q_UNUSED(index);
    updatePreview();
}

void GradientDialog::updatePreview()
{
    m_gradient = gradient();
    if (auto *preview = static_cast<GradientPreviewWidget *>(m_previewWidget))
        preview->setGradient(m_gradient);
}

QLinearGradient GradientDialog::gradient() const
{
    QLinearGradient g;

    switch (m_directionCombo->currentIndex()) {
    case LeftToRight:
        g = QLinearGradient(0, 0, 1, 0);
        break;
    case RightToLeft:
        g = QLinearGradient(1, 0, 0, 0);
        break;
    case TopToBottom:
        g = QLinearGradient(0, 0, 0, 1);
        break;
    case BottomToTop:
        g = QLinearGradient(0, 1, 0, 0);
        break;
    case DiagonalTL:
        g = QLinearGradient(0, 0, 1, 1);
        break;
    case DiagonalBR:
        g = QLinearGradient(1, 1, 0, 0);
        break;
    default:
        g = QLinearGradient(0, 0, 1, 0);
        break;
    }

    g.setCoordinateMode(QGradient::ObjectBoundingMode);
    g.setColorAt(0.0, m_startColor);
    g.setColorAt(1.0, m_endColor);

    return g;
}
