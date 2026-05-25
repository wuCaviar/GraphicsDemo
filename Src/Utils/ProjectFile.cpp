#include "ProjectFile.h"

#include <QBuffer>
#include <QDataStream>
#include <QFile>
#include <QFileInfo>

static const QByteArray kFileMagic = QByteArrayLiteral("ATP\x00");
static const int kFileHeaderSize = 4;

// ---- encryption -----------------------------------------------------------

QByteArray ProjectFile::encrypt(const QByteArray &data, const QByteArray &key)
{
    if (key.isEmpty())
        return data;

    QByteArray result = data;
    for (int i = 0; i < result.size(); ++i)
        result[i] = result[i] ^ key[i % key.size()];
    return result;
}

QByteArray ProjectFile::decrypt(const QByteArray &data, const QByteArray &key)
{
    return encrypt(data, key); // XOR is symmetric
}

// ---- save ------------------------------------------------------------------

bool ProjectFile::save(const QString &filePath, const ProjectInfo &info,
                       const CanvasInfo &canvas,
                       const QList<QGraphicsItem *> &items)
{
    m_lastError.clear();

    // 1. Build XML document
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
    for (auto *gi : items) {
        auto *igi = dynamic_cast<IGraphicsItem *>(gi);
        if (!igi)
            continue;
        itemsEl.appendChild(serializeItem(doc, gi, ++id));
    }

    QByteArray xmlData = doc.toByteArray();

    // 2. Encrypt with key derived from project name
    QByteArray key = QByteArrayLiteral("ATGraphics") + info.name.toUtf8();
    QByteArray encrypted = encrypt(xmlData, key);

    // 3. Write file (magic + encrypted payload)
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

// ---- load ------------------------------------------------------------------

bool ProjectFile::load(const QString &filePath, ProjectInfo &info,
                       CanvasInfo &canvas, QList<QGraphicsItem *> &items)
{
    m_lastError.clear();
    items.clear();

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        m_lastError = QStringLiteral("Cannot open file: %1").arg(filePath);
        return false;
    }

    // 1. Check magic header
    QByteArray header = file.read(kFileHeaderSize);
    if (header.size() != kFileHeaderSize || header != kFileMagic) {
        m_lastError = QStringLiteral("Invalid project file format");
        return false;
    }

    QByteArray encrypted = file.readAll();
    file.close();

    // 2. Derive key from file name (as a close approximation to project name)
    //    The key is derived from the project info after decryption — use a
    //    two-pass approach: first try with a default key, extract project name,
    //    then re-decrypt.

    // For simplicity, use the stored file's base name as the key approximation
    QFileInfo fi(filePath);
    QString baseName = fi.completeBaseName();
    QByteArray key = QByteArrayLiteral("ATGraphics") + baseName.toUtf8();

    QByteArray xmlData = decrypt(encrypted, key);

    // 3. Parse XML
    QDomDocument doc;
    auto parseXml = [&](const QByteArray &data) -> bool {
        auto result = doc.setContent(data);
        if (!result) {
            m_lastError = QStringLiteral("XML parse error at line %1: %2")
                              .arg(result.errorLine)
                              .arg(result.errorMessage);
            return false;
        }
        return true;
    };

    if (!parseXml(xmlData)) {
        // Try with empty key as fallback
        key.clear();
        xmlData = decrypt(encrypted, key);
        if (!parseXml(xmlData))
            return false;
    }

    QDomElement root = doc.documentElement();
    if (root.tagName() != QStringLiteral("Project")) {
        m_lastError = QStringLiteral("Unexpected root element: %1").arg(root.tagName());
        return false;
    }

    // Parse <Information>
    QDomElement infoEl = root.firstChildElement(QStringLiteral("Information"));
    if (!infoEl.isNull()) {
        info.name = infoEl.firstChildElement(QStringLiteral("Name")).text();
        info.version = infoEl.firstChildElement(QStringLiteral("Version")).text();
        info.author = infoEl.firstChildElement(QStringLiteral("Author")).text();
        info.description = infoEl.firstChildElement(QStringLiteral("Description")).text();
    }

    // Parse <Canvas>
    QDomElement canvasEl = root.firstChildElement(QStringLiteral("Canvas"));
    if (!canvasEl.isNull()) {
        canvas.width =
            canvasEl.firstChildElement(QStringLiteral("Width")).text().toDouble();
        canvas.height =
            canvasEl.firstChildElement(QStringLiteral("Height")).text().toDouble();
        canvas.dpi =
            canvasEl.firstChildElement(QStringLiteral("Dpi")).text().toDouble();
    }

    // Parse <Items>
    QDomElement itemsEl = canvasEl.firstChildElement(QStringLiteral("Items"));
    if (!itemsEl.isNull()) {
        QDomNodeList itemNodes = itemsEl.elementsByTagName(QStringLiteral("Item"));
        for (int i = 0; i < itemNodes.count(); ++i) {
            QDomElement itemEl = itemNodes.at(i).toElement();
            IGraphicsItem *igi = deserializeItem(itemEl);
            if (igi) {
                auto *gi = dynamic_cast<QGraphicsItem *>(igi);
                if (gi)
                    items.append(gi);
                else
                    delete igi;
            }
        }
    }

    return true;
}

// ---- item serialization helpers --------------------------------------------

QDomElement ProjectFile::serializeItem(QDomDocument &doc, QGraphicsItem *item,
                                       int id)
{
    auto *igi = dynamic_cast<IGraphicsItem *>(item);
    QDomElement el = doc.createElement(QStringLiteral("Item"));

    el.setAttribute(QStringLiteral("ID"), id);
    el.setAttribute(QStringLiteral("Type"), static_cast<int>(igi ? igi->itemType() : 0));
    el.setAttribute(QStringLiteral("Z"), item->zValue());
    el.setAttribute(QStringLiteral("X"), item->pos().x());
    el.setAttribute(QStringLiteral("Y"), item->pos().y());
    el.setAttribute(QStringLiteral("Rot"), item->rotation());

    if (igi) {
        // Serialize item data via QDataStream → Base64
        QByteArray binary;
        QDataStream out(&binary, QIODevice::WriteOnly);
        out << static_cast<int>(igi->itemType());
        igi->serialize(out);

        QDomElement dataEl = doc.createElement(QStringLiteral("Data"));
        dataEl.appendChild(doc.createTextNode(binary.toBase64()));
        el.appendChild(dataEl);
    }

    return el;
}

IGraphicsItem *ProjectFile::deserializeItem(const QDomElement &el)
{
    int typeVal = el.attribute(QStringLiteral("Type")).toInt();
    auto itemType = static_cast<IGraphicsItem::ItemType>(typeVal);

    IGraphicsItem *igi = createItemByType(itemType);
    if (!igi)
        return nullptr;

    QDomElement dataEl = el.firstChildElement(QStringLiteral("Data"));
    if (!dataEl.isNull()) {
        QByteArray binary = QByteArray::fromBase64(dataEl.text().toUtf8());
        QDataStream in(&binary, QIODevice::ReadOnly);

        int storedType;
        in >> storedType;
        if (storedType != typeVal || !igi->deserialize(in)) {
            delete igi;
            return nullptr;
        }
    }

    auto *gi = dynamic_cast<QGraphicsItem *>(igi);
    if (gi) {
        gi->setZValue(el.attribute(QStringLiteral("Z")).toDouble());
        gi->setPos(el.attribute(QStringLiteral("X")).toDouble(),
                   el.attribute(QStringLiteral("Y")).toDouble());
        gi->setRotation(el.attribute(QStringLiteral("Rot")).toDouble());
    }

    return igi;
}
