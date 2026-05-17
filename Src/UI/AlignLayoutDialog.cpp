#include "AlignLayoutDialog.h"
#include "AlignmentUtils.h"
#include "CanvasItem.h"
#include "Commands.h"
#include "ResizeHandleItem.h"

#include <algorithm>

#include <QDoubleSpinBox>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QShowEvent>
#include <QVBoxLayout>

// ============================================================
// 静态持久化状态（跨弹出保持）
// ============================================================
qreal  AlignLayoutDialog::s_hSpacing = 0.0;
qreal  AlignLayoutDialog::s_vSpacing = 0.0;
bool   AlignLayoutDialog::s_hAuto    = true;
bool   AlignLayoutDialog::s_vAuto    = true;

// ============================================================
// 构造
// ============================================================
AlignLayoutDialog::AlignLayoutDialog(QGraphicsScene *scene, QUndoStack *undoStack,
                                     QWidget *parent)
    : QDialog(parent), m_scene(scene), m_undoStack(undoStack)
{
    setWindowTitle(tr("Align & Layout"));
    setMinimumWidth(340);
    setupUI();
    refreshSelectionInfo();
}

// ============================================================
// showEvent — 每次弹出时刷新选中信息
// ============================================================
void AlignLayoutDialog::showEvent(QShowEvent *event)
{
    refreshSelectionInfo();
    QDialog::showEvent(event);
}

// ============================================================
// UI 构建
// ============================================================
void AlignLayoutDialog::setupUI()
{
    auto *mainLayout = new QVBoxLayout(this);

    // ---- Align Group ----
    auto *alignGroup = new QGroupBox(tr("Align"), this);
    auto *alignGrid = new QGridLayout(alignGroup);

    // 使用按钮替代 RadioButton，每个按钮点击即执行对齐
    m_alignLeftBtn    = new QPushButton(tr("Left"), this);
    m_alignHCenterBtn = new QPushButton(tr("HCenter"), this);
    m_alignRightBtn   = new QPushButton(tr("Right"), this);
    m_alignTopBtn     = new QPushButton(tr("Top"), this);
    m_alignVCenterBtn = new QPushButton(tr("VCenter"), this);
    m_alignBottomBtn  = new QPushButton(tr("Bottom"), this);

    // 设置按钮最小高度和工具提示
    auto alignButtons = {m_alignLeftBtn, m_alignHCenterBtn, m_alignRightBtn,
                         m_alignTopBtn, m_alignVCenterBtn, m_alignBottomBtn};
    for (auto *btn : alignButtons) {
        btn->setMinimumHeight(28);
        btn->setToolTip(tr("Click to align selected items"));
    }

    alignGrid->addWidget(m_alignLeftBtn,    0, 0);
    alignGrid->addWidget(m_alignHCenterBtn, 0, 1);
    alignGrid->addWidget(m_alignRightBtn,   0, 2);
    alignGrid->addWidget(m_alignTopBtn,     1, 0);
    alignGrid->addWidget(m_alignVCenterBtn, 1, 1);
    alignGrid->addWidget(m_alignBottomBtn,  1, 2);

    mainLayout->addWidget(alignGroup);

    // ---- Distribute Group ----
    auto *distGroup = new QGroupBox(tr("Distribute"), this);
    auto *distLayout = new QVBoxLayout(distGroup);

    auto *distBtnRow = new QHBoxLayout;
    m_distHBtn = new QPushButton(tr("Distribute Horizontally"), this);
    m_distVBtn = new QPushButton(tr("Distribute Vertically"), this);
    m_distHBtn->setMinimumHeight(28);
    m_distVBtn->setMinimumHeight(28);
    m_distHBtn->setToolTip(tr("Click to distribute selected items horizontally"));
    m_distVBtn->setToolTip(tr("Click to distribute selected items vertically"));
    distBtnRow->addWidget(m_distHBtn);
    distBtnRow->addWidget(m_distVBtn);
    distLayout->addLayout(distBtnRow);

    // ---- Spacing ----
    auto *spacingWidget = new QWidget(this);
    auto *spacingLayout = new QHBoxLayout(spacingWidget);
    spacingLayout->setContentsMargins(20, 4, 0, 4);

    // 水平间距
    auto *hGroup = new QHBoxLayout;
    m_hSpacingSpin = new QDoubleSpinBox(this);
    m_hSpacingSpin->setRange(0.0, 10000.0);
    m_hSpacingSpin->setSingleStep(1.0);
    m_hSpacingSpin->setDecimals(1);
    m_hSpacingSpin->setSuffix(tr(" px"));
    m_hSpacingSpin->setValue(s_hSpacing);

    m_hAutoBtn = new QPushButton(this);
    m_hAutoBtn->setCheckable(true);
    m_hAutoBtn->setChecked(s_hAuto);
    m_hAutoBtn->setFixedWidth(60);
    m_hAutoBtn->setToolTip(tr("Toggle auto spacing"));
    updateAutoBtnText(m_hAutoBtn);
    m_hSpacingSpin->setEnabled(!s_hAuto);

    hGroup->addWidget(new QLabel(tr("H:"), this));
    hGroup->addWidget(m_hSpacingSpin);
    hGroup->addWidget(m_hAutoBtn);

    // 垂直间距
    auto *vGroup = new QHBoxLayout;
    m_vSpacingSpin = new QDoubleSpinBox(this);
    m_vSpacingSpin->setRange(0.0, 10000.0);
    m_vSpacingSpin->setSingleStep(1.0);
    m_vSpacingSpin->setDecimals(1);
    m_vSpacingSpin->setSuffix(tr(" px"));
    m_vSpacingSpin->setValue(s_vSpacing);

    m_vAutoBtn = new QPushButton(this);
    m_vAutoBtn->setCheckable(true);
    m_vAutoBtn->setToolTip(tr("Toggle auto spacing"));
    m_vAutoBtn->setChecked(s_vAuto);
    m_vAutoBtn->setFixedWidth(60);
    updateAutoBtnText(m_vAutoBtn);
    m_vSpacingSpin->setEnabled(!s_vAuto);

    vGroup->addWidget(new QLabel(tr("V:"), this));
    vGroup->addWidget(m_vSpacingSpin);
    vGroup->addWidget(m_vAutoBtn);

    spacingLayout->addLayout(hGroup);
    spacingLayout->addLayout(vGroup);

    distLayout->addWidget(spacingWidget);
    mainLayout->addWidget(distGroup);

    // ---- 选中信息 ----
    m_selectionInfoLabel = new QLabel(tr("Selected: 0 items"), this);
    mainLayout->addWidget(m_selectionInfoLabel);

    // ---- Close 按钮 ----
    auto *closeBtn = new QPushButton(tr("Close"), this);
    closeBtn->setDefault(false);
    closeBtn->setAutoDefault(false);
    closeBtn->setToolTip(tr("Close the dialog"));
    connect(closeBtn, &QPushButton::clicked, this, &QDialog::close);
    mainLayout->addWidget(closeBtn, 0, Qt::AlignRight);

    // ---- 信号连接 ----
    // 对齐按钮
    connect(m_alignLeftBtn, &QPushButton::clicked, this, [this]() { onAlignButtonClicked(); });
    connect(m_alignHCenterBtn, &QPushButton::clicked, this, [this]() { onAlignButtonClicked(); });
    connect(m_alignRightBtn, &QPushButton::clicked, this, [this]() { onAlignButtonClicked(); });
    connect(m_alignTopBtn, &QPushButton::clicked, this, [this]() { onAlignButtonClicked(); });
    connect(m_alignVCenterBtn, &QPushButton::clicked, this, [this]() { onAlignButtonClicked(); });
    connect(m_alignBottomBtn, &QPushButton::clicked, this, [this]() { onAlignButtonClicked(); });

    // 分布按钮
    connect(m_distHBtn, &QPushButton::clicked, this, &AlignLayoutDialog::onDistributeButtonClicked);
    connect(m_distVBtn, &QPushButton::clicked, this, &AlignLayoutDialog::onDistributeButtonClicked);

    // Auto 按钮 toggle
    connect(m_hAutoBtn, &QPushButton::toggled, this, &AlignLayoutDialog::onAutoSpacingToggled);
    connect(m_vAutoBtn, &QPushButton::toggled, this, &AlignLayoutDialog::onAutoSpacingToggled);

    // 间距 SpinBox 值变化时保存
    connect(m_hSpacingSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, [](double val) { s_hSpacing = val; });
    connect(m_vSpacingSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, [](double val) { s_vSpacing = val; });

    // 选中变化时刷新信息
    if (m_scene) {
        connect(m_scene, &QGraphicsScene::selectionChanged,
                this, &AlignLayoutDialog::onSelectionChanged);
    }
}

// ============================================================
// 更新 Auto 按钮文字
// ============================================================
void AlignLayoutDialog::updateAutoBtnText(QPushButton *btn)
{
    btn->setText(btn->isChecked() ? tr("Auto") : tr("Manual"));
}

// ============================================================
// Auto 间距按钮 toggle
// ============================================================
void AlignLayoutDialog::onAutoSpacingToggled()
{
    if (sender() == m_hAutoBtn) {
        s_hAuto = m_hAutoBtn->isChecked();
        m_hSpacingSpin->setEnabled(!s_hAuto);
        if (s_hAuto)
            m_hSpacingSpin->setValue(0);
        updateAutoBtnText(m_hAutoBtn);
    } else {
        s_vAuto = m_vAutoBtn->isChecked();
        m_vSpacingSpin->setEnabled(!s_vAuto);
        if (s_vAuto)
            m_vSpacingSpin->setValue(0);
        updateAutoBtnText(m_vAutoBtn);
    }
}

// ============================================================
// 选中变化时刷新信息
// ============================================================
void AlignLayoutDialog::onSelectionChanged()
{
    refreshSelectionInfo();
}

// ============================================================
// 刷新选中信息
// ============================================================
void AlignLayoutDialog::refreshSelectionInfo()
{
    if (!m_scene) return;
    int count = filterSelectableItems().size();
    m_selectionInfoLabel->setText(tr("Selected: %1 item(s)").arg(count));
}

// ============================================================
// 过滤可选中的图元
// ============================================================
QList<QGraphicsItem *> AlignLayoutDialog::filterSelectableItems() const
{
    if (!m_scene) return {};
    return ::filterSelectableItems(m_scene->selectedItems());
}

// ============================================================
// 对齐按钮点击 — 立即执行对齐，不关闭对话框
// ============================================================
void AlignLayoutDialog::onAlignButtonClicked()
{
    AlignmentUtils::AlignDirection direction;
    QObject *senderObj = sender();
    if (senderObj == m_alignLeftBtn)       direction = AlignmentUtils::AlignLeft;
    else if (senderObj == m_alignHCenterBtn) direction = AlignmentUtils::AlignHCenter;
    else if (senderObj == m_alignRightBtn)  direction = AlignmentUtils::AlignRight;
    else if (senderObj == m_alignTopBtn)    direction = AlignmentUtils::AlignTop;
    else if (senderObj == m_alignVCenterBtn) direction = AlignmentUtils::AlignVCenter;
    else if (senderObj == m_alignBottomBtn) direction = AlignmentUtils::AlignBottom;
    else return;

    applyAlign(direction);
    refreshSelectionInfo();
}

// ============================================================
// 分布按钮点击 — 立即执行分布，不关闭对话框
// ============================================================
void AlignLayoutDialog::onDistributeButtonClicked()
{
    AlignmentUtils::DistributeDirection direction;
    if (sender() == m_distHBtn) direction = AlignmentUtils::DistributeH;
    else                        direction = AlignmentUtils::DistributeV;

    applyDistribute(direction);
    refreshSelectionInfo();
}

// ============================================================
// 执行对齐 — 委托给 AlignmentUtils
// ============================================================
bool AlignLayoutDialog::applyAlign(AlignmentUtils::AlignDirection direction)
{
    auto items = filterSelectableItems();
    if (items.size() < 2) return false;

    auto result = AlignmentUtils::computeAlign(items, direction);
    if (!result.valid) return false;

    m_undoStack->push(new AlignItemsCommand(
        items, result.oldPositions, result.newPositions,
        tr("Align %1").arg(AlignmentUtils::alignDirectionName(direction)),
        m_scene));
    return true;
}

// ============================================================
// 执行分布 — 委托给 AlignmentUtils
// ============================================================
bool AlignLayoutDialog::applyDistribute(AlignmentUtils::DistributeDirection direction)
{
    auto items = filterSelectableItems();
    if (items.size() < 2) return false;

    // 构建分布参数
    AlignmentUtils::DistributeParams params;
    if (direction == AlignmentUtils::DistributeH) {
        params.autoSpacing = s_hAuto;
        params.customSpacing = s_hSpacing;
    } else {
        params.autoSpacing = s_vAuto;
        params.customSpacing = s_vSpacing;
    }

    auto result = AlignmentUtils::computeDistribute(items, direction, params);
    if (!result.valid) return false;

    m_undoStack->push(new AlignItemsCommand(
        items, result.oldPositions, result.newPositions,
        tr("Distribute %1").arg(AlignmentUtils::distributeDirectionName(direction)),
        m_scene));
    return true;
}
