#include "ProjectFile.h"

#include <QDataStream>
#include <QFile>
#include <QFileInfo>

static const QByteArray kFileMagic = QByteArrayLiteral("ATP\x00");
static const int kFileHeaderSize = 4;

// ---- thread-safe worker functions ------------------------------------------

SerializedItem serializeItemWorker(QGraphicsItem *item)
{
    SerializedItem result;
    auto *igi = dynamic_cast<IGraphicsItem *>(item);

    result.itemType = igi ? static_cast<int>(igi->itemType()) : 0;
    result.zValue = item->zValue();
    result.posX = item->pos().x();
    result.posY = item->pos().y();
    result.rotation = item->rotation();

    if (igi) {
        QByteArray binary;
        QDataStream out(&binary, QIODevice::WriteOnly);
        out << static_cast<int>(igi->itemType());
        igi->serialize(out);
        result.base64Data = binary.toBase64();
    }

    return result;
}

IGraphicsItem *deserializeItemWorker(const DeserialTask &task)
{
    auto itemType = static_cast<IGraphicsItem::ItemType>(task.itemType);
    IGraphicsItem *igi = createItemByType(itemType);
    if (!igi)
        return nullptr;

    QByteArray binary = QByteArray::fromBase64(task.base64Data);
    QDataStream in(&binary, QIODevice::ReadOnly);

    int storedType = 0;
    in >> storedType;
    if (storedType != task.itemType || !igi->deserialize(in)) {
        delete igi;
        return nullptr;
    }

    auto *gi = dynamic_cast<QGraphicsItem *>(igi);
    if (gi) {
        gi->setZValue(task.zValue);
        gi->setPos(task.posX, task.posY);
        gi->setRotation(task.rotation);
    }

    return igi;
}

// ---- encryption ------------------------------------------------------------

static QByteArray xorData(const QByteArray &data, const QByteArray &key)
{
    if (key.isEmpty())
        return data;

    QByteArray result = data;
    for (int i = 0; i < result.size(); ++i)
        result[i] = result[i] ^ key[i % key.size()];
    return result;
}

QByteArray ProjectFile::encrypt(const QByteArray &data, const QByteArray &key)
{
    return xorData(data, key);
}

QByteArray ProjectFile::decrypt(const QByteArray &data, const QByteArray &key)
{
    return xorData(data, key);
}

// ---- save (from pre-serialized items) --------------------------------------

bool ProjectFile::saveFromSerialized(const QString &filePath,
                                     const ProjectInfo &info,
                                     const CanvasInfo &canvas,
                                     const QList<SerializedItem> &items)
{
    m_lastError.clear();

    // Build XML document
    QDomDocument doc;
    QDomProcessingInstruction pi =
        doc.createProcessingInstruction(QStringLiteral("xml"),
                                        QStringLiteral("version=\"1.0\" encoding=\"UTF-8\""));
    doc.appendChild(pi);

    QDomElement root = doc.createElement(QStringLiteral("Project"));
    doc.appendChild(root);

    // <Information>
    QDomElement infoEl = doc.createElement(QStringLiteral("Information"));
    root.appendChild(infoEl);
    auto addTextElem = [&](const QString &tag, const QString &value) {
        QDomElement el = doc.createElement(tag);
        el.appendChild(doc.createTextNode(value));
        infoEl.appendChild(el);
    };
    addTextElem(QStringLiteral("Name"), info.name);
    addTextElem(QStringLiteral("Version"), info.version);
    addTextElem(QStringLiteral("Author"), info.author);
    addTextElem(QStringLiteral("Description"), info.description);

    // <Canvas>
    QDomElement canvasEl = doc.createElement(QStringLiteral("Canvas"));
    root.appendChild(canvasEl);
    auto addCanvasProp = [&](const QString &tag, double value) {
        QDomElement el = doc.createElement(tag);
        el.appendChild(doc.createTextNode(QString::number(value, 'f', 2)));
        canvasEl.appendChild(el);
    };
    addCanvasProp(QStringLiteral("Width"), canvas.width);
    addCanvasProp(QStringLiteral("Height"), canvas.height);
    addCanvasProp(QStringLiteral("Dpi"), canvas.dpi);

    // <Items>
    QDomElement itemsEl = doc.createElement(QStringLiteral("Items"));
    canvasEl.appendChild(itemsEl);

    int id = 0;
    for (const auto &si : items) {
        QDomElement itemEl = doc.createElement(QStringLiteral("Item"));
        itemEl.setAttribute(QStringLiteral("ID"), ++id);
        itemEl.setAttribute(QStringLiteral("Type"), si.itemType);
        itemEl.setAttribute(QStringLiteral("Z"), si.zValue);
        itemEl.setAttribute(QStringLiteral("X"), si.posX);
        itemEl.setAttribute(QStringLiteral("Y"), si.posY);
        itemEl.setAttribute(QStringLiteral("Rot"), si.rotation);

        if (!si.base64Data.isEmpty()) {
            QDomElement dataEl = doc.createElement(QStringLiteral("Data"));
            dataEl.appendChild(doc.createTextNode(QString::fromLatin1(si.base64Data)));
            itemEl.appendChild(dataEl);
        }

        itemsEl.appendChild(itemEl);
    }

    QByteArray xmlData = doc.toByteArray();

    // Encrypt and write
    QByteArray key = QByteArrayLiteral("ATGraphics") + info.name.toUtf8();
    QByteArray encrypted = encrypt(xmlData, key);

    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly)) {
        m_lastError = QStringLiteral("Cannot write file: %1").arg(filePath);
        return false;
    }

    file.write(kFileMagic);
    file.write(encrypted);
    file.close();

    return true;
}

// ---- parse for deserialize -------------------------------------------------

bool ProjectFile::parseForDeserialize(const QString &filePath,
                                      ProjectInfo &info, CanvasInfo &canvas,
                                      QList<DeserialTask> &tasks)
{
    m_lastError.clear();
    tasks.clear();

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        m_lastError = QStringLiteral("Cannot open file: %1").arg(filePath);
        return false;
    }

    QByteArray header = file.read(kFileHeaderSize);
    if (header.size() != kFileHeaderSize || header != kFileMagic) {
        m_lastError = QStringLiteral("Invalid project file format");
        return false;
    }

    QByteArray encrypted = file.readAll();
    file.close();

    QFileInfo fi(filePath);
    QByteArray key = QByteArrayLiteral("ATGraphics") + fi.completeBaseName().toUtf8();

    QByteArray xmlData = decrypt(encrypted, key);

    // Parse XML
    QDomDocument doc;
    auto parseResult = doc.setContent(xmlData);
    if (!parseResult) {
        // Try with empty key
        key.clear();
        xmlData = decrypt(encrypted, key);
        parseResult = doc.setContent(xmlData);
        if (!parseResult) {
            m_lastError = QStringLiteral("XML parse error at line %1: %2")
                              .arg(parseResult.errorLine)
                              .arg(parseResult.errorMessage);
            return false;
        }
    }

    QDomElement root = doc.documentElement();
    if (root.tagName() != QStringLiteral("Project")) {
        m_lastError =
            QStringLiteral("Unexpected root element: %1").arg(root.tagName());
        return false;
    }

    // <Information>
    QDomElement infoEl = root.firstChildElement(QStringLiteral("Information"));
    if (!infoEl.isNull()) {
        info.name = infoEl.firstChildElement(QStringLiteral("Name")).text();
        info.version = infoEl.firstChildElement(QStringLiteral("Version")).text();
        info.author = infoEl.firstChildElement(QStringLiteral("Author")).text();
        info.description =
            infoEl.firstChildElement(QStringLiteral("Description")).text();
    }

    // <Canvas>
    QDomElement canvasEl = root.firstChildElement(QStringLiteral("Canvas"));
    if (!canvasEl.isNull()) {
        canvas.width =
            canvasEl.firstChildElement(QStringLiteral("Width")).text().toDouble();
        canvas.height =
            canvasEl.firstChildElement(QStringLiteral("Height")).text().toDouble();
        canvas.dpi =
            canvasEl.firstChildElement(QStringLiteral("Dpi")).text().toDouble();
    }

    // <Items> → extract DeserialTask list
    QDomElement itemsEl = canvasEl.firstChildElement(QStringLiteral("Items"));
    if (!itemsEl.isNull()) {
        QDomNodeList itemNodes = itemsEl.elementsByTagName(QStringLiteral("Item"));
        for (int i = 0; i < itemNodes.count(); ++i) {
            QDomElement el = itemNodes.at(i).toElement();

            DeserialTask task;
            task.itemType = el.attribute(QStringLiteral("Type")).toInt();
            task.zValue = el.attribute(QStringLiteral("Z")).toDouble();
            task.posX = el.attribute(QStringLiteral("X")).toDouble();
            task.posY = el.attribute(QStringLiteral("Y")).toDouble();
            task.rotation = el.attribute(QStringLiteral("Rot")).toDouble();

            QDomElement dataEl = el.firstChildElement(QStringLiteral("Data"));
            if (!dataEl.isNull())
                task.base64Data = dataEl.text().toLatin1();

            tasks.append(task);
        }
    }

    return true;
}

// ---- synchronous save (fallback) -------------------------------------------

bool ProjectFile::save(const QString &filePath, const ProjectInfo &info,
                       const CanvasInfo &canvas,
                       const QList<QGraphicsItem *> &items)
{
    QList<SerializedItem> serialized;
    serialized.reserve(items.size());
    for (auto *item : items)
        serialized.append(serializeItemWorker(item));
    return saveFromSerialized(filePath, info, canvas, serialized);
}

// ---- synchronous load (fallback) -------------------------------------------

bool ProjectFile::load(const QString &filePath, ProjectInfo &info,
                       CanvasInfo &canvas, QList<QGraphicsItem *> &items)
{
    QList<DeserialTask> tasks;
    if (!parseForDeserialize(filePath, info, canvas, tasks))
        return false;

    items.clear();
    items.reserve(tasks.size());
    for (const auto &task : tasks) {
        IGraphicsItem *igi = deserializeItemWorker(task);
        if (igi) {
            auto *gi = dynamic_cast<QGraphicsItem *>(igi);
            if (gi)
                items.append(gi);
            else
                delete igi;
        }
    }
    return true;
}
