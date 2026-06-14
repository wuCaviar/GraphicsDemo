#ifndef ALIGNLAYOUTDIALOG_H
#define ALIGNLAYOUTDIALOG_H

#include <QDockWidget>
#include <QPointer>
#include <QGraphicsItem>

#include "AlignmentUtils.h"

class QGraphicsScene;
class QUndoStack;
class QDoubleSpinBox;
class QPushButton;
class QLabel;

class AlignLayoutDialog : public QDockWidget
{
    Q_OBJECT

public:
    explicit AlignLayoutDialog(QGraphicsScene *scene, QUndoStack *undoStack,
                               QWidget *parent = nullptr);

    void setScene(QGraphicsScene *scene);
    void setUndoStack(QUndoStack *undoStack);
    void refreshSelectionInfo();

    // Session persistence — exposed for session serialization
    static qreal hSpacing()    { return s_hSpacing; }
    static void setHSpacing(qreal v) { s_hSpacing = v; }
    static qreal vSpacing()    { return s_vSpacing; }
    static void setVSpacing(qreal v) { s_vSpacing = v; }

private slots:
    void onAlignClicked();
    void onDistributeClicked();
    void onPageCenterClicked();
    void onSelectionChanged();

private:
    void setupUI(QWidget *container);
    QList<QGraphicsItem *> filterSelectableItems() const;

    bool applyAlign(AlignmentUtils::AlignDirection direction);
    bool applyDistribute(AlignmentUtils::DistributeDirection direction,
                         const AlignmentUtils::DistributeParams &params);
    bool applyPageCenter(AlignmentUtils::AlignDirection direction);
    bool applyStretchAlign(AlignmentUtils::AlignDirection direction);

    QPointer<QGraphicsScene> m_scene;
    QUndoStack *m_undoStack = nullptr;

    // Horizontal align
    QPushButton *m_hAlignLeft = nullptr;
    QPushButton *m_hAlignCenter = nullptr;
    QPushButton *m_hAlignRight = nullptr;
    QPushButton *m_hAlignStretch = nullptr;
    QPushButton *m_hAlignProp = nullptr;

    // Horizontal distribute
    QPushButton *m_hDistLeft = nullptr;
    QPushButton *m_hDistCenter = nullptr;
    QPushButton *m_hDistRight = nullptr;
    QPushButton *m_hDistEqualGap = nullptr;
    QPushButton *m_hDistCustom = nullptr;

    // Vertical align
    QPushButton *m_vAlignTop = nullptr;
    QPushButton *m_vAlignCenter = nullptr;
    QPushButton *m_vAlignBottom = nullptr;
    QPushButton *m_vAlignStretch = nullptr;
    QPushButton *m_vAlignProp = nullptr;

    // Vertical distribute
    QPushButton *m_vDistTop = nullptr;
    QPushButton *m_vDistCenter = nullptr;
    QPushButton *m_vDistBottom = nullptr;
    QPushButton *m_vDistEqualGap = nullptr;
    QPushButton *m_vDistCustom = nullptr;

    // Page center
    QPushButton *m_pageHCenter = nullptr;
    QPushButton *m_pageVCenter = nullptr;

    // Spacing
    QDoubleSpinBox *m_hSpacingSpin = nullptr;
    QDoubleSpinBox *m_vSpacingSpin = nullptr;

    // Info
    QLabel *m_selectionInfoLabel = nullptr;

    // Persisted spacing values
    static qreal s_hSpacing;
    static qreal s_vSpacing;
};

#endif // ALIGNLAYOUTDIALOG_H
