#include "ProjectDocument.h"
#include "QAtCanvasPage.h"
#include "IGraphicsItem.h"
#include "CanvasItem.h"

#include <QDataStream>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QGraphicsScene>
#include <QGraphicsItem>

// ============================================================
//  构造
// ============================================================

ProjectDocument::ProjectDocument(QObject *parent) : QObject(parent) { }

// ============================================================
//  路径管理
// ============================================================

void ProjectDocument::setFilePath(const QString &path)
{
    if (m_filePath == path)
        return;
    m_filePath = path;
    emit filePathChanged(path);
    emit displayNameChanged(displayName());
}

QString ProjectDocument::displayName() const
{
    if (m_filePath.isEmpty())
        return tr("Untitled project");
    return QFileInfo(m_filePath).completeBaseName();
}

// ============================================================
//  修改追踪
// ============================================================

bool ProjectDocument::isModified() const
{
    for (auto it = m_canvasStates.begin(); it != m_canvasStates.end(); ++it) {
        if (it.value().modified)
            return true;
    }
    return false;
}

bool ProjectDocument::isCanvasModified(QAtCanvasPage *page) const
{
    return m_canvasStates.value(page).modified;
}

void ProjectDocument::markCanvasModified(QAtCanvasPage *page, bool modified)
{
    if (!m_canvasStates.contains(page))
        return;
    bool wasModified = isModified();
    m_canvasStates[page].modified = modified;
    if (wasModified != isModified())
        emit modifiedChanged(isModified());
}

void ProjectDocument::markAllClean()
{
    bool wasModified = isModified();
    for (auto it = m_canvasStates.begin(); it != m_canvasStates.end(); ++it)
        it.value().modified = false;
    if (wasModified)
        emit modifiedChanged(false);
}

bool ProjectDocument::hasAnyContent() const
{
    for (auto it = m_canvasStates.begin(); it != m_canvasStates.end(); ++it) {
        auto *page = it.key();
        if (!page || !page->scene())
            continue;
        auto items = filterSelectableItems(page->scene()->items());
        if (!items.isEmpty())
            return true;
    }
    return false;
}

// ============================================================
//  画布状态管理
// ============================================================

void ProjectDocument::registerCanvas(QAtCanvasPage *page)
{
    if (!page || m_canvasStates.contains(page))
        return;
    m_canvasStates[page] = CanvasState();
}

void ProjectDocument::unregisterCanvas(QAtCanvasPage *page)
{
    if (!page)
        return;
    bool wasModified = isModified();
    m_canvasStates.remove(page);
    if (wasModified != isModified())
        emit modifiedChanged(isModified());
}

CanvasState ProjectDocument::canvasState(QAtCanvasPage *page) const
{
    return m_canvasStates.value(page);
}

QList<QAtCanvasPage *> ProjectDocument::canvases() const
{
    return m_canvasStates.keys();
}

// ============================================================
//  备份链
// ============================================================

void ProjectDocument::rotateBackups() const
{
    rotateBackups(m_filePath, m_maxBackups);
}

void ProjectDocument::rotateBackups(const QString &filePath, int maxBackups)
{
    if (filePath.isEmpty() || maxBackups <= 0)
        return;

    // 删除最旧的备份 (atpN)
    QString oldest = filePath + QString::number(maxBackups);
    QFile::remove(oldest);

    // 轮转: atp2 → atp3, atp1 → atp2, atp → atp1
    for (int i = maxBackups - 1; i >= 1; --i) {
        QString src = (i == 1) ? filePath : filePath + QString::number(i - 1);
        QString dst = filePath + QString::number(i);
        QFile::remove(dst);
        QFile::rename(src, dst);
    }
}

// ============================================================
//  序列化收集
// ============================================================

QList<CanvasSaveBundle> ProjectDocument::collectBundles() const
{
    QList<CanvasSaveBundle> bundles;
    for (auto it = m_canvasStates.begin(); it != m_canvasStates.end(); ++it) {
        auto *page = it.key();
        if (!page || !page->canvasItem())
            continue;

        CanvasSaveBundle bundle;
        bundle.info.width = page->canvasItem()->canvasSize().width();
        bundle.info.height = page->canvasItem()->canvasSize().height();
        bundle.info.dpi =
            page->canvasItem()->isDpiLocked() ? page->canvasItem()->canvasDpiX() : 0.0;

        auto items = filterSelectableItems(page->scene()->items());
        for (auto *item : items) {
            SerializeInput input;
            auto *igi = dynamic_cast<IGraphicsItem *>(item);
            input.itemType = igi ? static_cast<int>(igi->itemType()) : 0;
            input.zValue = item->zValue();
            input.posX = item->pos().x();
            input.posY = item->pos().y();
            input.rotation = item->rotation();
            if (igi) {
                QByteArray binary;
                QDataStream out(&binary, QIODevice::WriteOnly);
                out << static_cast<int>(igi->itemType());
                igi->serialize(out);
                input.binary = binary;

                if (igi->hasPenCmyk()) {
                    input.cmyk.hasPen = true;
                    igi->penCmyk(input.cmyk.penC, input.cmyk.penM, input.cmyk.penY,
                                 input.cmyk.penK);
                }
                if (igi->hasBrushCmyk()) {
                    input.cmyk.hasBrush = true;
                    igi->brushCmyk(input.cmyk.brushC, input.cmyk.brushM, input.cmyk.brushY,
                                   input.cmyk.brushK);
                }
                input.cmyk.gradient = igi->gradientStopCmykMap();
            }
            bundle.items.append(serializeItemWorker(input));
        }
        bundles.append(bundle);
    }
    return bundles;
}
