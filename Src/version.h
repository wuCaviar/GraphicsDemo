#ifndef VERSION_H
#define VERSION_H

#include <QString>

constexpr int monthAbbrToInt(const char *abbr)
{
    if (abbr[0] == 'J' && abbr[1] == 'a')
        return 1; // Jan
    if (abbr[0] == 'F')
        return 2; // Feb
    if (abbr[0] == 'M' && abbr[2] == 'r')
        return 3; // Mar
    if (abbr[0] == 'A' && abbr[1] == 'p')
        return 4; // Apr
    if (abbr[0] == 'M' && abbr[2] == 'y')
        return 5; // May
    if (abbr[0] == 'J' && abbr[1] == 'u' && abbr[2] == 'n')
        return 6; // Jun
    if (abbr[0] == 'J' && abbr[1] == 'u' && abbr[2] == 'l')
        return 7; // Jul
    if (abbr[0] == 'A' && abbr[1] == 'u')
        return 8; // Aug
    if (abbr[0] == 'S')
        return 9; // Sep
    if (abbr[0] == 'O')
        return 10; // Oct
    if (abbr[0] == 'N')
        return 11; // Nov
    if (abbr[0] == 'D')
        return 12; // Dec
    return 0;
}

constexpr int cstr2int_skip(const char *str, int len)
{
    int val = 0;
    int i = 0;
    while (i < len && (str[i] < '0' || str[i] > '9'))
        ++i;
    while (i < len && str[i] >= '0' && str[i] <= '9') {
        val = val * 10 + (str[i] - '0');
        ++i;
    }
    return val;
}

inline QString buildVersion()
{
    constexpr const char *date = __DATE__;
    constexpr int month = monthAbbrToInt(date);
    constexpr int day = cstr2int_skip(date + 4, 2);
    constexpr int year = cstr2int_skip(date + 7, 4);

    constexpr const char *time = __TIME__;
    constexpr int hour = cstr2int_skip(time, 2);
    constexpr int min = cstr2int_skip(time + 3, 2);

    return QString("%1%2%3%4%5")
        .arg(year, 4, 10, QChar('0'))
        .arg(month, 2, 10, QChar('0'))
        .arg(day, 2, 10, QChar('0'))
        .arg(hour, 2, 10, QChar('0'))
        .arg(min, 2, 10, QChar('0'));
}

inline QString appVersion()
{
    static const QString ver = buildVersion();
    return ver;
}

static QString RipVersion = "Unknown";

#endif // VERSION_H
