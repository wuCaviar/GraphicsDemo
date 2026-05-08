#ifndef ALIGNLAYOUTDIALOG_H
#define ALIGNLAYOUTDIALOG_H

#include <QDialog>
#include <QPointer>
#include <QGraphicsItem>
#include <QMap>

#include "AlignmentUtils.h"

class QGraphicsScene;
class QUndoStack;
class QDoubleSpinBox;
class QPushButton;
class QLabel;

// 对齐与布局对话框
// - 对齐/分布/间距均用按钮，点击立即生效（对话框不关闭）
// - 无选中图元时只记录设置
// - 关闭后再次弹出时保持上次设置
class AlignLayoutDialog : public QDialog
{
    Q_OBJECT

public:
    explicit AlignLayoutDialog(QGraphicsScene *scene, QUndoStack *undoStack,
                               QWidget *parent = nullptr);

    void refreshSelectionInfo();

protected:
    void showEvent(QShowEvent *event) override;

private slots:
    void onAlignButtonClicked();
    void onDistributeButtonClicked();
    void onAutoSpacingToggled();
    void onSelectionChanged();

private:
    void setupUI();
    void updateAutoBtnText(QPushButton *btn);
    QList<QGraphicsItem *> filterSelectableItems() const;

    // 执行对齐，返回是否成功
    bool applyAlign(AlignmentUtils::AlignDirection direction);
    // 执行分布，返回是否成功
    bool applyDistribute(AlignmentUtils::DistributeDirection direction);

    QPointer<QGraphicsScene> m_scene;
    QUndoStack *m_undoStack = nullptr;

    // ---- Align buttons ----
    QPushButton *m_alignLeftBtn     = nullptr;
    QPushButton *m_alignHCenterBtn  = nullptr;
    QPushButton *m_alignRightBtn    = nullptr;
    QPushButton *m_alignTopBtn       = nullptr;
    QPushButton *m_alignVCenterBtn  = nullptr;
    QPushButton *m_alignBottomBtn   = nullptr;

    // ---- Distribute buttons ----
    QPushButton *m_distHBtn = nullptr;
    QPushButton *m_distVBtn = nullptr;

    // ---- Spacing ----
    QDoubleSpinBox *m_hSpacingSpin = nullptr;
    QDoubleSpinBox *m_vSpacingSpin = nullptr;
    QPushButton *m_hAutoBtn = nullptr;   // toggle button: Auto / Manual
    QPushButton *m_vAutoBtn = nullptr;

    // ---- Info ----
    QLabel *m_selectionInfoLabel = nullptr;

    // ---- Persisted state (跨弹出保持) ----
    static qreal s_hSpacing;
    static qreal s_vSpacing;
    static bool s_hAuto;
    static bool s_vAuto;
};

#endif // ALIGNLAYOUTDIALOG_H
