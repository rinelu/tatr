#ifndef LIB_UTIL_H_
#define LIB_UTIL_H_

#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#if defined(__GNUC__) || defined(__clang__)
#    ifdef __MINGW_PRINTF_FORMAT
#        define FMT(STRING_INDEX, FIRST_TO_CHECK) __attribute__ ((format (__MINGW_PRINTF_FORMAT, STRING_INDEX, FIRST_TO_CHECK)))
#    else
#        define FMT(STRING_INDEX, FIRST_TO_CHECK) __attribute__ ((format (printf, STRING_INDEX, FIRST_TO_CHECK)))
#    endif // __MINGW_PRINTF_FORMAT
#endif

#ifdef __cplusplus
#    define DELCTYPE(T) (decltype(T))
#else
#    define DELCTYPE(T)
#endif // __cplusplus

inline static void timestamp_id(char *buf, size_t bufsz)
{
    time_t t = time(NULL);
    struct tm tm = *localtime(&t);
    strftime(buf, bufsz, "%Y%m%d-%H%M%S", &tm);
}

inline static void get_timestamp(time_t t, char *buf, size_t sz)
{
    struct tm *tm = localtime(&t);
    strftime(buf, sz, "%Y-%m-%dT%H:%M:%S", tm);
}

inline static void timestamp_iso(char *buf, size_t bufsz)
{
    time_t t = time(NULL);
    get_timestamp(t, buf, bufsz);
}

inline static bool str_in(const char *s, const char *const *list)
{
    if (!s || !list) return false;

    for (size_t i = 0; list[i]; i++) {
        if (strcmp(s, list[i]) == 0)
            return true;
    }

    return false;
}

inline static int cmp_paths(const void *a, const void *b)
{
    const char *pa = *(const char *const *)a;
    const char *pb = *(const char *const *)b;
    return strcmp(pa, pb);
}

inline static time_t parse_time(const char *s)
{
    struct tm tm = {0};

    if (sscanf(s, "%d-%d-%dT%d:%d:%d",
               &tm.tm_year, &tm.tm_mon, &tm.tm_mday,
               &tm.tm_hour, &tm.tm_min, &tm.tm_sec) != 6)
        return 0; // invalid

    tm.tm_year -= 1900;
    tm.tm_mon  -= 1;

    return mktime(&tm);
}

inline static time_t parse_time_n(const char *s, size_t len)
{
    char buf[32];
    if (len >= sizeof(buf)) len = sizeof(buf) - 1;
    memcpy(buf, s, len);
    buf[len] = '\0';
    return parse_time(buf);
}

inline static time_t parse_date(const char *s)
{
    if (!s || !*s) return 0;
    time_t t = parse_time(s);
    if (t != 0) return t;

    struct tm tm = {0};
    if (sscanf(s, "%d-%d-%d", &tm.tm_year, &tm.tm_mon, &tm.tm_mday) == 3) {
        tm.tm_year -= 1900;
        tm.tm_mon  -= 1;
        tm.tm_isdst = -1;
        return mktime(&tm);
    }
    return 0;
}



#endif // LIB_UTIL_H_
