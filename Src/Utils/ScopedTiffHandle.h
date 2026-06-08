#ifndef SCOPEDTIFFHANDLE_H
#define SCOPEDTIFFHANDLE_H

#include <tiffio.h>

namespace ImageUtils {

// RAII wrapper for TIFF* handles
class ScopedTiffHandle
{
public:
    ScopedTiffHandle() = default;

    explicit ScopedTiffHandle(const char *path, const char *mode)
        : m_tif(TIFFOpen(path, mode))
    {
    }

    ~ScopedTiffHandle() { close(); }

    ScopedTiffHandle(const ScopedTiffHandle &) = delete;
    ScopedTiffHandle &operator=(const ScopedTiffHandle &) = delete;

    ScopedTiffHandle(ScopedTiffHandle &&other) noexcept
        : m_tif(other.m_tif)
    {
        other.m_tif = nullptr;
    }

    ScopedTiffHandle &operator=(ScopedTiffHandle &&other) noexcept
    {
        if (this != &other) {
            close();
            m_tif = other.m_tif;
            other.m_tif = nullptr;
        }
        return *this;
    }

    TIFF *get() const { return m_tif; }
    TIFF *operator->() const { return m_tif; }
    explicit operator bool() const { return m_tif != nullptr; }

    void close()
    {
        if (m_tif) {
            TIFFClose(m_tif);
            m_tif = nullptr;
        }
    }

    TIFF *release()
    {
        TIFF *t = m_tif;
        m_tif = nullptr;
        return t;
    }

private:
    TIFF *m_tif = nullptr;
};

} // namespace ImageUtils

#endif // SCOPEDTIFFHANDLE_H
