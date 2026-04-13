#ifndef LIB_UTIL_H_
#define LIB_UTIL_H_

#include <stdbool.h>
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

inline static void timestamp_iso(char *buf, size_t bufsz)
{
    time_t t = time(NULL);
    struct tm tm = *localtime(&t);
    strftime(buf, bufsz, "%Y-%m-%dT%H:%M:%S", &tm);
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


#endif // LIB_UTIL_H_
