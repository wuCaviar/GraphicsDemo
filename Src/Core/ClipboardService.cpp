#include "ClipboardService.h"
#include "Commands/Commands.h"
#include "IGraphicsItem.h"

#include <QApplication>
#include <QClipboard>
#include <QDataStream>
#include <QGraphicsItem>
#include <QGraphicsScene>
#include <QMimeData>
#include <QUndoStack>

static const char *kMimeTypeStr = "application/x-graphicsdemo-items";

ClipboardService::ClipboardService(QObject *parent) : QAtService(parent) {}

QString ClipboardService::description() const { return tr("Clipboard management service"); }
QString ClipboardService::mimeType() { return QString::fromLatin1(kMimeTypeStr); }

void ClipboardService::copy(const QList<QGraphicsItem*> &items)
{
    if (items.isEmpty()) return;

    QByteArray data;
    QDataStream out(&data, QIODevice::WriteOnly);
    out << IGraphicsItem::kSerializationVersion;

    auto filtered = filterSelectableItems(items);
    int count = 0;
    for (auto *item : filtered)
        if (dynamic_cast<IGraphicsItem*>(item)) count++;

    out << count;
    for (auto *item : filtered) {
        auto *gi = dynamic_cast<IGraphicsItem*>(item);
        if (!gi) continue;
        QByteArray itemBinary = serializeItemToBytes(gi);
        out << static_cast<quint32>(itemBinary.size());
        out.writeRawData(itemBinary.constData(), itemBinary.size());
    }

    auto *mime = new QMimeData;
    mime->setData(mimeType(), data);
    QApplication::clipboard()->setMimeData(mime);
}

void ClipboardService::cut(const QList<QGraphicsItem*> &items,
                            QGraphicsScene *scene, QUndoStack *undoStack)
{
    if (items.isEmpty()) return;
    if (!scene || !undoStack) return;

    undoStack->beginMacro(tr("Cut"));
    copy(items);
    auto deletable = filterSelectableItems(items);
    if (!deletable.isEmpty())
        undoStack->push(new RemoveItemsCommand(scene, deletable));
    undoStack->endMacro();
}

QList<QGraphicsItem*> ClipboardService::paste(QGraphicsScene *scene, QUndoStack *undoStack)
{
    QList<QGraphicsItem*> result;
    Q_UNUSED(undoStack)

    const QMimeData *mime = QApplication::clipboard()->mimeData();
    if (!mime || !mime->hasFormat(mimeType())) return result;

    QByteArray data = mime->data(mimeType());
    QDataStream in(&data, QIODevice::ReadOnly);

    int version = 0;
    in >> version;
    if (version < 1 || version > IGraphicsItem::kSerializationVersion) {
        qWarning("Clipboard: unsupported format version %d (current: %d)",
                 version, IGraphicsItem::kSerializationVersion);
        return result;
    }

    int count = 0;
    in >> count;
    for (int i = 0; i < count; ++i) {
        quint32 dataLen = 0;
        in >> dataLen;
        if (in.status() != QDataStream::Ok || dataLen == 0) break;

        QByteArray itemData(static_cast<int>(dataLen), '\0');
        in.readRawData(itemData.data(), static_cast<int>(dataLen));
        if (in.status() != QDataStream::Ok) break;

        QDataStream itemIn(&itemData, QIODevice::ReadOnly);
        int typeInt = 0;
        itemIn >> typeInt;
        auto *gi = createItemByType(static_cast<IGraphicsItem::ItemType>(typeInt));
        if (!gi) continue;
        if (!gi->deserialize(itemIn)) { delete gi; continue; }
        auto *qgi = dynamic_cast<QGraphicsItem*>(gi);
        if (qgi) { result << qgi; if (scene) scene->addItem(qgi); }
    }
    return result;
}
