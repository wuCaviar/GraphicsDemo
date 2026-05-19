#ifndef TIFFFILE_H
#define TIFFFILE_H

#include <QScopedPointer>
#include <QVector>
#include <QVariant>
#include <QExplicitlySharedDataPointer>

class QByteArray;
class TiffIfdEntryPrivate;
class TiffIfdPrivate;
class TiffFilePrivate;

struct TiffParserOptions
{
    bool parserSubIfds{ true };
};

class TiffIfdEntry
{
public:
    enum Tag
    {
        T_SubFleType = 254,
        T_ImageWidth = 256,
        T_ImageLength = 257,
        T_Compression = 259,
        T_SubIfd = 330,
        T_Photoshop = 34377,
    };

    enum DataType
    {
        DT_Byte = 1,
        DT_Ascii,
        DT_Short,
        DT_Long,
        DT_Rational,
        DT_SByte,
        DT_Undefined,
        DT_SShort,
        DT_SLong,
        DT_SRational,
        DT_Float,
        DT_Double,
        DT_Ifd,
        DT_Long8,
        DT_SLong8,
        DT_Ifd8
    };

    TiffIfdEntry();
    TiffIfdEntry(const TiffIfdEntry &other);
    ~TiffIfdEntry();

    quint16 tag() const;
    QString tagName() const;
    quint16 type() const;
    QString typeName() const;
    quint64 count() const;
    QByteArray valueOrOffset() const;
    QVariantList values() const;
    QString valueDescription() const;
    bool isValid() const;

private:
    friend class TiffFilePrivate;
    QExplicitlySharedDataPointer<TiffIfdEntryPrivate> d;
};

class TiffIfd
{
public:
    TiffIfd();
    TiffIfd(const TiffIfd &other);
    ~TiffIfd();

    QVector<TiffIfdEntry> ifdEntries() const;
    QVector<TiffIfd> subIfds() const;
    qint64 nextIfdOffset() const;
    bool isValid() const;

    TiffIfdEntry ifdEntry(quint16 tag) const;

private:
    friend class TiffFilePrivate;
    QExplicitlySharedDataPointer<TiffIfdPrivate> d;
};

class TiffFile
{
public:
    enum ByteOrder
    {
        LittleEndian,
        BigEndian
    };

    TiffFile(const QString &filePath, const TiffParserOptions &options);
    ~TiffFile();

    QString errorString() const;
    bool hasError() const;

    // header information
    QByteArray headerBytes() const;
    bool isBigTiff() const;
    ByteOrder byteOrder() const;
    int version() const;
    qint64 ifd0Offset() const;

    // ifds
    QVector<TiffIfd> ifds() const;

private:
    QScopedPointer<TiffFilePrivate> d;
};

#endif // TIFFFILE_H