#ifndef COMMONDEFS_H
#define COMMONDEFS_H

#ifdef _WIN32
// define something for Windows (32-bit and 64-bit, this part is common)
#    ifdef _WIN64
// define something for Windows (64-bit only)
#    else
// define something for Windows (32-bit only)
#    endif

#elif __APPLE__
#    if TARGET_IPHONE_SIMULATOR
// iOS Simulator
#    elif TARGET_OS_IPHONE
// iOS device
#    elif TARGET_OS_MAC
// Other kinds of Mac OS
#    endif

#elif __ANDROID__
// android
#elif __linux__
// linux
#endif

#include <algorithm>
#include <cstring>
#include <deque>
#include <functional>
#include <list>
#include <map>
#include <memory>
#include <set>
#include <stack>
#include <unordered_map>
#include <vector>
#include <wchar.h>
#include <string>
using namespace std;

typedef unsigned char uchar;
typedef unsigned int uint;
typedef unsigned short ushort;
typedef wchar_t tchar;
typedef unsigned long dword, ulong;
typedef unsigned long long ulonglong;
typedef string String;
typedef shared_ptr<void> VoidPtr;
typedef long long int64;
typedef unsigned long long uint64;

// #define VENDOR_NAME "Caviar"

// #define BEGIN_SEVEN_NAMESPACE \
//     namespace Caviar {        \
//         namespace Seven {
// #define END_SEVEN_NAMESPACE \
//     }                       \
//     }
// #define USING_NAMESPACE_SEVEN using namespace Caviar::Seven;

#define CLASS_TYPEDEFS(_name_) \
    class _name_;              \
    typedef shared_ptr<_name_> _name_##Ptr;

#define FORWARD_DECLARE(_name_)             \
    class _name_;                           \
    typedef shared_ptr<_name_> _name_##Ptr; \
    typedef weak_ptr<_name_> _name_##WPtr;

#define ENABLE_MAKE_SHARED(_name_)                                                   \
private:                                                                             \
    template<typename _name_, typename... Args>                                      \
    static shared_ptr<_name_> create(Args &&...args)                                 \
    {                                                                                \
        class make_shared_enabler : public _name_                                    \
        {                                                                            \
        public:                                                                      \
            make_shared_enabler(Args &&...args) : _name_(forward<Args>(args)...) { } \
        };                                                                           \
        return make_shared<make_shared_enabler>(forward<Args>(args)...);             \
    }

#define MAKE_SHARED(_name_, ...) _name_::create<_name_>(__VA_ARGS__)

#define __T(x) L##x
#define _T(x) __T(x)

#define STATIC_CAST(_name_, _obj_) static_pointer_cast<_name_>(_obj_)
#define DYNAMIC_CAST(_name_, _obj_) dynamic_pointer_cast<_name_>(_obj_)

#define ENABLE_SINGLETON(_name_)      \
private:                              \
    _name_(const _name_ &) = delete;  \
    _name_(const _name_ &&) = delete; \
    _name_ &operator=(const _name_ &) = delete;

#ifdef __GNUC__

#    define _countof(_Array_) (sizeof(_Array_) / sizeof(_Array_[0]))

inline int _wtoi(wchar_t const *_String)
{
    return wcstol(_String, nullptr, 10);
}

inline double _wtof(wchar_t const *_String)
{
    return wcstod(_String, nullptr);
}

inline int memcpy_s(void *dest, size_t destsz, const void *src, size_t count)
{
    if (dest == nullptr || src == nullptr) {
        return EINVAL;
    }
    if (count > destsz) {
        memset(dest, 0, destsz);
        return ERANGE;
    }
    memcpy(dest, src, count);
    return 0;
}

inline int _wcsicmp(wchar_t const *_String1, wchar_t const *_String2)
{
    return wcscasecmp(_String1, _String2);
}

// 跨平台兼容的 _vscprintf 实现
inline int _vscprintf(const char *format, va_list args)
{
    int len = vsnprintf(nullptr, 0, format, args);
    return (len >= 0) ? len : -1;
}

// 跨平台兼容的 _vscprintf 实现
inline int _vsnprintf_s(char *pszStr, size_t nLength, size_t /*nLength1*/, const char *lpcszFormat,
                        va_list args)
{
    // 调用 vsnprintf，强制限制写入大小为 bufferSize-1
    int result = vsnprintf(pszStr, nLength, lpcszFormat, args);
    if (result < 0) {
        // 错误处理（如编码错误）
        return -1;
    }
    return result; // 返回写入的字符数（不包括 '\0'）
}

#endif

#endif // COMMONDEFS_H
