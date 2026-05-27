#include "AlignLayoutDialog.h"
#include "AlignmentUtils.h"
#include "CanvasItem.h"
#include "Commands.h"
#include "ResizeHandleItem.h"

#include <QDoubleSpinBox>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
#include <QWidget>

// ============================================================
// Static persisted state
// ============================================================
qreal AlignLayoutDialog::s_hSpacing = 0.0;
qreal AlignLayoutDialog::s_vSpacing = 0.0;

// ============================================================
// Helper: create an icon-only button from resource path
// ============================================================
static QPushButton *makeBtn(const QString &iconPath, const QString &tooltip,
                            QWidget *parent)
{
    auto *btn = new QPushButton(parent);
    btn->setIcon(QIcon(iconPath));
    btn->setIconSize(QSize(24, 24));
    btn->setToolTip(tooltip);
    btn->setMinimumSize(36, 36);
    btn->setMaximumSize(48, 48);
    btn->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    btn->setFocusPolicy(Qt::NoFocus);
    btn->setFlat(true);
    return btn;
}

#define ICON(name) QStringLiteral(":/icons/icons/" name ".svg")

// ============================================================
// Constructor
// ============================================================
AlignLayoutDialog::AlignLayoutDialog(QGraphicsScene *scene,
                                     QUndoStack *undoStack, QWidget *parent)
    : QDockWidget(parent), m_scene(scene), m_undoStack(undoStack)
{
    setWindowTitle(tr("Align & Distribute"));
    setAllowedAreas(Qt::RightDockWidgetArea | Qt::LeftDockWidgetArea);

    auto *container = new QWidget(this);
    setWidget(container);
    setupUI(container);
    refreshSelectionInfo();
}

// ============================================================
// UI Setup
// ============================================================
void AlignLayoutDialog::setupUI(QWidget *container)
{
    auto *mainLayout = new QVBoxLayout(container);
    mainLayout->setContentsMargins(4, 4, 4, 4);
    mainLayout->setSpacing(4);

    // ---- Horizontal Group ----
    auto *hGroup = new QGroupBox(tr("Horizontal"), container);
    auto *hLayout = new QVBoxLayout(hGroup);
    hLayout->setContentsMargins(4, 8, 4, 4);
    hLayout->setSpacing(2);

    auto *hAlignRow = new QHBoxLayout;
    hAlignRow->setSpacing(2);
    m_hAlignLeft =
        makeBtn(ICON("align-left"), tr("Align left edges"), container);
    m_hAlignCenter = makeBtn(ICON("align-hcenter"),
                             tr("Align horizontal centers"), container);
    m_hAlignRight =
        makeBtn(ICON("align-right"), tr("Align right edges"), container);
    m_hAlignStretch =
        makeBtn(ICON("align-stretch-h"),
                tr("Stretch to same width, align left & right"), container);
    m_hAlignProp =
        makeBtn(ICON("align-prop-h"),
                tr("Stretch horizontally, scale proportionally"), container);
    hAlignRow->addWidget(m_hAlignLeft);
    hAlignRow->addWidget(m_hAlignCenter);
    hAlignRow->addWidget(m_hAlignRight);
    hAlignRow->addWidget(m_hAlignStretch);
    hAlignRow->addWidget(m_hAlignProp);
    hAlignRow->addStretch();
    hLayout->addLayout(hAlignRow);

    auto *hDistRow = new QHBoxLayout;
    hDistRow->setSpacing(2);
    m_hDistLeft =
        makeBtn(ICON("dist-left"), tr("Left edges equally spaced"), container);
    m_hDistCenter = makeBtn(ICON("dist-hcenter"),
                            tr("Horizontal centers equally spaced"), container);
    m_hDistRight = makeBtn(ICON("dist-right"), tr("Right edges equally spaced"),
                           container);
    m_hDistEqualGap =
        makeBtn(ICON("dist-hequal"), tr("Equal horizontal gaps"), container);
    m_hDistCustom = makeBtn(ICON("dist-hcustom"),
                            tr("Custom horizontal gap spacing"), container);
    hDistRow->addWidget(m_hDistLeft);
    hDistRow->addWidget(m_hDistCenter);
    hDistRow->addWidget(m_hDistRight);
    hDistRow->addWidget(m_hDistEqualGap);
    hDistRow->addWidget(m_hDistCustom);
    hDistRow->addStretch();
    hLayout->addLayout(hDistRow);

    auto *hSpRow = new QHBoxLayout;
    m_hSpacingSpin = new QDoubleSpinBox(container);
    m_hSpacingSpin->setRange(0.0, 10000.0);
    m_hSpacingSpin->setSingleStep(1.0);
    m_hSpacingSpin->setDecimals(1);
    m_hSpacingSpin->setSuffix(tr(" px"));
    m_hSpacingSpin->setValue(s_hSpacing);
    m_hSpacingSpin->setToolTip(tr("Gap value for Custom Gap distribution"));
    hSpRow->addWidget(new QLabel(tr("Gap:"), container));
    hSpRow->addWidget(m_hSpacingSpin);
    hSpRow->addStretch();
    hLayout->addLayout(hSpRow);

    mainLayout->addWidget(hGroup);

    // ---- Vertical Group ----
    auto *vGroup = new QGroupBox(tr("Vertical"), container);
    auto *vLayout = new QVBoxLayout(vGroup);
    vLayout->setContentsMargins(4, 8, 4, 4);
    vLayout->setSpacing(2);

    auto *vAlignRow = new QHBoxLayout;
    vAlignRow->setSpacing(2);
    m_vAlignTop = makeBtn(ICON("align-top"), tr("Align top edges"), container);
    m_vAlignCenter =
        makeBtn(ICON("align-vcenter"), tr("Align vertical centers"), container);
    m_vAlignBottom =
        makeBtn(ICON("align-bottom"), tr("Align bottom edges"), container);
    m_vAlignStretch =
        makeBtn(ICON("align-stretch-v"),
                tr("Stretch to same height, align top & bottom"), container);
    m_vAlignProp =
        makeBtn(ICON("align-prop-v"),
                tr("Stretch vertically, scale proportionally"), container);
    vAlignRow->addWidget(m_vAlignTop);
    vAlignRow->addWidget(m_vAlignCenter);
    vAlignRow->addWidget(m_vAlignBottom);
    vAlignRow->addWidget(m_vAlignStretch);
    vAlignRow->addWidget(m_vAlignProp);
    vAlignRow->addStretch();
    vLayout->addLayout(vAlignRow);

    auto *vDistRow = new QHBoxLayout;
    vDistRow->setSpacing(2);
    m_vDistTop =
        makeBtn(ICON("dist-top"), tr("Top edges equally spaced"), container);
    m_vDistCenter = makeBtn(ICON("dist-vcenter"),
                            tr("Vertical centers equally spaced"), container);
    m_vDistBottom = makeBtn(ICON("dist-bottom"),
                            tr("Bottom edges equally spaced"), container);
    m_vDistEqualGap =
        makeBtn(ICON("dist-vequal"), tr("Equal vertical gaps"), container);
    m_vDistCustom = makeBtn(ICON("dist-vcustom"),
                            tr("Custom vertical gap spacing"), container);
    vDistRow->addWidget(m_vDistTop);
    vDistRow->addWidget(m_vDistCenter);
    vDistRow->addWidget(m_vDistBottom);
    vDistRow->addWidget(m_vDistEqualGap);
    vDistRow->addWidget(m_vDistCustom);
    vDistRow->addStretch();
    vLayout->addLayout(vDistRow);

    auto *vSpRow = new QHBoxLayout;
    m_vSpacingSpin = new QDoubleSpinBox(container);
    m_vSpacingSpin->setRange(0.0, 10000.0);
    m_vSpacingSpin->setSingleStep(1.0);
    m_vSpacingSpin->setDecimals(1);
    m_vSpacingSpin->setSuffix(tr(" px"));
    m_vSpacingSpin->setValue(s_vSpacing);
    m_vSpacingSpin->setMaximumWidth(90);
    m_vSpacingSpin->setToolTip(tr("Gap value for Custom Gap distribution"));
    vSpRow->addWidget(new QLabel(tr("Gap:"), container));
    vSpRow->addWidget(m_vSpacingSpin);
    vSpRow->addStretch();
    vLayout->addLayout(vSpRow);

    mainLayout->addWidget(vGroup);

    // ---- Page Group ----
    auto *pageGroup = new QGroupBox(tr("Page"), container);
    auto *pageLayout = new QHBoxLayout(pageGroup);
    pageLayout->setContentsMargins(4, 8, 4, 4);
    pageLayout->setSpacing(2);
    m_pageHCenter = makeBtn(ICON("page-hcenter"),
                            tr("Center horizontally on canvas"), container);
    m_pageVCenter = makeBtn(ICON("page-vcenter"),
                            tr("Center vertically on canvas"), container);
    pageLayout->addWidget(m_pageHCenter);
    pageLayout->addWidget(m_pageVCenter);
    pageLayout->addStretch();
    mainLayout->addWidget(pageGroup);

    // ---- Bottom: selection info ----
    auto *bottomRow = new QHBoxLayout;
    m_selectionInfoLabel = new QLabel(tr("Selected: 0 items"), container);
    bottomRow->addWidget(m_selectionInfoLabel);
    bottomRow->addStretch();
    mainLayout->addLayout(bottomRow);

    // ---- Signal connections ----
    connect(m_hAlignLeft, &QPushButton::clicked, this,
            &AlignLayoutDialog::onAlignClicked);
    connect(m_hAlignCenter, &QPushButton::clicked, this,
            &AlignLayoutDialog::onAlignClicked);
    connect(m_hAlignRight, &QPushButton::clicked, this,
            &AlignLayoutDialog::onAlignClicked);
    connect(m_hAlignStretch, &QPushButton::clicked, this,
            &AlignLayoutDialog::onAlignClicked);
    connect(m_hAlignProp, &QPushButton::clicked, this,
            &AlignLayoutDialog::onAlignClicked);
    connect(m_vAlignTop, &QPushButton::clicked, this,
            &AlignLayoutDialog::onAlignClicked);
    connect(m_vAlignCenter, &QPushButton::clicked, this,
            &AlignLayoutDialog::onAlignClicked);
    connect(m_vAlignBottom, &QPushButton::clicked, this,
            &AlignLayoutDialog::onAlignClicked);
    connect(m_vAlignStretch, &QPushButton::clicked, this,
            &AlignLayoutDialog::onAlignClicked);
    connect(m_vAlignProp, &QPushButton::clicked, this,
            &AlignLayoutDialog::onAlignClicked);

    connect(m_hDistLeft, &QPushButton::clicked, this,
            &AlignLayoutDialog::onDistributeClicked);
    connect(m_hDistCenter, &QPushButton::clicked, this,
            &AlignLayoutDialog::onDistributeClicked);
    connect(m_hDistRight, &QPushButton::clicked, this,
            &AlignLayoutDialog::onDistributeClicked);
    connect(m_hDistEqualGap, &QPushButton::clicked, this,
            &AlignLayoutDialog::onDistributeClicked);
    connect(m_hDistCustom, &QPushButton::clicked, this,
            &AlignLayoutDialog::onDistributeClicked);
    connect(m_vDistTop, &QPushButton::clicked, this,
            &AlignLayoutDialog::onDistributeClicked);
    connect(m_vDistCenter, &QPushButton::clicked, this,
            &AlignLayoutDialog::onDistributeClicked);
    connect(m_vDistBottom, &QPushButton::clicked, this,
            &AlignLayoutDialog::onDistributeClicked);
    connect(m_vDistEqualGap, &QPushButton::clicked, this,
            &AlignLayoutDialog::onDistributeClicked);
    connect(m_vDistCustom, &QPushButton::clicked, this,
            &AlignLayoutDialog::onDistributeClicked);

    connect(m_pageHCenter, &QPushButton::clicked, this,
            &AlignLayoutDialog::onPageCenterClicked);
    connect(m_pageVCenter, &QPushButton::clicked, this,
            &AlignLayoutDialog::onPageCenterClicked);

    connect(m_hSpacingSpin,
            QOverload<double>::of(&QDoubleSpinBox::valueChanged), this,
            [](double val) { s_hSpacing = val; });
    connect(m_vSpacingSpin,
            QOverload<double>::of(&QDoubleSpinBox::valueChanged), this,
            [](double val) { s_vSpacing = val; });

    if (m_scene) {
        connect(m_scene, &QGraphicsScene::selectionChanged, this,
                &AlignLayoutDialog::onSelectionChanged);
    }
}

// ============================================================
// Selection info
// ============================================================
void AlignLayoutDialog::onSelectionChanged()
{
    refreshSelectionInfo();
}

void AlignLayoutDialog::refreshSelectionInfo()
{
    if (!m_scene)
        return;
    int count = filterSelectableItems().size();
    m_selectionInfoLabel->setText(tr("Selected: %1 item(s)").arg(count));
}

QList<QGraphicsItem *> AlignLayoutDialog::filterSelectableItems() const
{
    if (!m_scene)
        return {};
    return ::filterSelectableItems(m_scene->selectedItems());
}

// ============================================================
// Handlers
// ============================================================
void AlignLayoutDialog::onAlignClicked()
{
    auto *btn = qobject_cast<QPushButton *>(sender());
    if (!btn)
        return;

    AlignmentUtils::AlignDirection dir;
    bool isStretch = false;

    if (btn == m_hAlignLeft)
        dir = AlignmentUtils::AlignLeft;
    else if (btn == m_hAlignCenter)
        dir = AlignmentUtils::AlignHCenter;
    else if (btn == m_hAlignRight)
        dir = AlignmentUtils::AlignRight;
    else if (btn == m_hAlignStretch) {
        dir = AlignmentUtils::AlignLeftRight;
        isStretch = true;
    } else if (btn == m_hAlignProp) {
        dir = AlignmentUtils::AlignHProportional;
        isStretch = true;
    } else if (btn == m_vAlignTop)
        dir = AlignmentUtils::AlignTop;
    else if (btn == m_vAlignCenter)
        dir = AlignmentUtils::AlignVCenter;
    else if (btn == m_vAlignBottom)
        dir = AlignmentUtils::AlignBottom;
    else if (btn == m_vAlignStretch) {
        dir = AlignmentUtils::AlignTopBottom;
        isStretch = true;
    } else if (btn == m_vAlignProp) {
        dir = AlignmentUtils::AlignVProportional;
        isStretch = true;
    } else
        return;

    if (isStretch)
        applyStretchAlign(dir);
    else
        applyAlign(dir);
    refreshSelectionInfo();
}

void AlignLayoutDialog::onDistributeClicked()
{
    auto *btn = qobject_cast<QPushButton *>(sender());
    if (!btn)
        return;

    AlignmentUtils::DistributeDirection dir;
    AlignmentUtils::DistributeParams params;

    if (btn == m_hDistLeft) {
        dir = AlignmentUtils::DistributeHLeft;
        params.autoSpacing = true;
    } else if (btn == m_hDistCenter) {
        dir = AlignmentUtils::DistributeHCenter;
        params.autoSpacing = true;
    } else if (btn == m_hDistRight) {
        dir = AlignmentUtils::DistributeHRight;
        params.autoSpacing = true;
    } else if (btn == m_hDistEqualGap) {
        dir = AlignmentUtils::DistributeH;
        params.autoSpacing = true;
    } else if (btn == m_hDistCustom) {
        dir = AlignmentUtils::DistributeH;
        params.autoSpacing = false;
        params.customSpacing = s_hSpacing;
    } else if (btn == m_vDistTop) {
        dir = AlignmentUtils::DistributeVTop;
        params.autoSpacing = true;
    } else if (btn == m_vDistCenter) {
        dir = AlignmentUtils::DistributeVCenter;
        params.autoSpacing = true;
    } else if (btn == m_vDistBottom) {
        dir = AlignmentUtils::DistributeVBottom;
        params.autoSpacing = true;
    } else if (btn == m_vDistEqualGap) {
        dir = AlignmentUtils::DistributeV;
        params.autoSpacing = true;
    } else if (btn == m_vDistCustom) {
        dir = AlignmentUtils::DistributeV;
        params.autoSpacing = false;
        params.customSpacing = s_vSpacing;
    } else {
        return;
    }

    applyDistribute(dir, params);
    refreshSelectionInfo();
}

void AlignLayoutDialog::onPageCenterClicked()
{
    auto *btn = qobject_cast<QPushButton *>(sender());
    if (!btn)
        return;

    AlignmentUtils::AlignDirection dir;
    if (btn == m_pageHCenter)
        dir = AlignmentUtils::AlignHCenterOnPage;
    else if (btn == m_pageVCenter)
        dir = AlignmentUtils::AlignVCenterOnPage;
    else
        return;

    applyPageCenter(dir);
    refreshSelectionInfo();
}

// ============================================================
// Apply operations
// ============================================================
bool AlignLayoutDialog::applyAlign(AlignmentUtils::AlignDirection direction)
{
    auto items = filterSelectableItems();
    if (items.size() < 2)
        return false;

    auto result = AlignmentUtils::computeAlign(items, direction);
    if (!result.valid)
        return false;

    m_undoStack->push(new AlignItemsCommand(
        items, result.oldPositions, result.newPositions,
        AlignmentUtils::alignDirectionName(direction), m_scene));
    return true;
}

bool AlignLayoutDialog::applyStretchAlign(
    AlignmentUtils::AlignDirection direction)
{
    auto items = filterSelectableItems();
    if (items.size() < 2)
        return false;

    auto result = AlignmentUtils::computeStretchAlign(items, direction);
    if (!result.valid)
        return false;

    m_undoStack->push(new StretchAlignItemsCommand(
        items, result.oldPositions, result.newPositions, result.oldGeometries,
        result.newGeometries, AlignmentUtils::alignDirectionName(direction),
        m_scene));
    return true;
}

bool AlignLayoutDialog::applyPageCenter(
    AlignmentUtils::AlignDirection direction)
{
    auto items = filterSelectableItems();
    if (items.isEmpty())
        return false;

    QRectF pageRect = m_scene->sceneRect();
    auto result = AlignmentUtils::computePageCenter(items, direction, pageRect);
    if (!result.valid)
        return false;

    m_undoStack->push(new AlignItemsCommand(
        items, result.oldPositions, result.newPositions,
        AlignmentUtils::alignDirectionName(direction), m_scene));
    return true;
}

bool AlignLayoutDialog::applyDistribute(
    AlignmentUtils::DistributeDirection direction,
    const AlignmentUtils::DistributeParams &params)
{
    auto items = filterSelectableItems();
    if (items.size() < 2)
        return false;

    auto result = AlignmentUtils::computeDistribute(items, direction, params);
    if (!result.valid)
        return false;

    m_undoStack->push(new AlignItemsCommand(
        items, result.oldPositions, result.newPositions,
        AlignmentUtils::distributeDirectionName(direction), m_scene));
    return true;
}
