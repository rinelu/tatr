/* log.h - v1.0.0 - Public Domain

   A single-header logging for C (C99+)
  
   Features
     - Coloured terminal output (auto-detected; disable with LOG_NO_COLOR)
     - Plain-text (ANSI-stripped) output when writing to a log file
     - Thread-safe on Linux / macOS (flockfile) and Windows (CRITICAL_SECTION)
     - Optional timestamps and source-file/line annotations
     - log_fatal() calls exit(1) after printing
     - log_confirm() interactive y/N prompt
  
   Quick Example
   ```c
     #include "log.h"
  
     int main(void) {
         log_init(.show_time = true, .show_file = true);
         log_info("Hello %s", "world");
         log_warn("Something looks off");
         log_fatal("Unrecoverable: %d", errno);
     }
    ```
  
   Optional File Output
   ```c
     FILE *f = fopen("app.log", "a");
     // console gets colour, file gets plain text
     log_init(.file = f);
   ```
 */

#ifndef LOG_H_
#define LOG_H_

#include <stdio.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#if defined(__GNUC__) || defined(__clang__)
#    ifdef __MINGW_PRINTF_FORMAT
#        define LOG_FMT(si, fc) __attribute__((format(__MINGW_PRINTF_FORMAT, si, fc)))
#    else
#        define LOG_FMT(si, fc) __attribute__((format(printf, si, fc)))
#    endif
#else
#    define LOG_FMT(si, fc)
#endif

#if defined(_WIN32) || defined(_WIN64)
#    ifndef WIN32_LEAN_AND_MEAN
#        define WIN32_LEAN_AND_MEAN
#    endif
#    include <windows.h>
#    include <io.h>
#    define LOG__ISATTY(f) _isatty(_fileno(f))
#    define LOG__USE_WINCRIT 1
#else
#    include <unistd.h>
#    define LOG__ISATTY(f) isatty(fileno(f))
#endif

#ifdef LOG_NO_COLOR
#    define A_RESET        ""
#    define A_BOLD         ""
#    define A_DIM          ""
#    define A_ITALIC       ""
#    define A_UNDERLINE    ""
#    define A_RED          ""
#    define A_GREEN        ""
#    define A_YELLOW       ""
#    define A_BLUE         ""
#    define A_MAGENTA      ""
#    define A_CYAN         ""
#    define A_WHITE        ""
#    define A_BRED         ""
#    define A_BGREEN       ""
#    define A_BYELLOW      ""
#    define A_BBLUE        ""
#    define A_BCYAN        ""
#    define A_BOLD_RED     ""
#    define A_BOLD_BRED    ""
#    define A_BOLD_GREEN   ""
#    define A_BOLD_YELLOW  ""
#    define A_BOLD_BLUE    ""
#    define A_BOLD_MAGENTA ""
#    define A_BOLD_CYAN    ""
#    define A_BOLD_WHITE   ""
#    define A_DIM_WHITE    ""
#else
#    define A_CSI "\x1b["
#    define A_RESET        A_CSI "0m"
#    define A_BOLD         A_CSI "1m"
#    define A_DIM          A_CSI "2m"
#    define A_ITALIC       A_CSI "3m"
#    define A_UNDERLINE    A_CSI "4m"
#    define A_RED          A_CSI "31m"
#    define A_GREEN        A_CSI "32m"
#    define A_YELLOW       A_CSI "33m"
#    define A_BLUE         A_CSI "34m"
#    define A_MAGENTA      A_CSI "35m"
#    define A_CYAN         A_CSI "36m"
#    define A_WHITE        A_CSI "37m"
#    define A_BRED         A_CSI "91m"
#    define A_BGREEN       A_CSI "92m"
#    define A_BYELLOW      A_CSI "93m"
#    define A_BBLUE        A_CSI "94m"
#    define A_BCYAN        A_CSI "96m"
#    define A_BOLD_RED     A_BOLD A_RED
#    define A_BOLD_BRED    A_BOLD A_BRED
#    define A_BOLD_GREEN   A_BOLD A_GREEN
#    define A_BOLD_YELLOW  A_BOLD A_YELLOW
#    define A_BOLD_BLUE    A_BOLD A_BLUE
#    define A_BOLD_MAGENTA A_BOLD A_MAGENTA
#    define A_BOLD_CYAN    A_BOLD A_CYAN
#    define A_BOLD_WHITE   A_BOLD A_WHITE
#    define A_DIM_WHITE    A_DIM  A_WHITE
#endif // LOG_NO_COLOR

#ifndef LOGDEF
#    define LOGDEF
#endif

typedef enum {
    LOG_NPRE = 0,
    LOG_DEBUG,
    LOG_INFO,
    LOG_WARN,
    LOG_ERROR,
    LOG_FATAL,
    LOG_NOTE,
    LOG_HINT
} Log_Level;

typedef struct {
    bool      use_color; // force color on (auto-detected if false)
    Log_Level level;     // minimum level printed
    bool      show_time; // prepend HH:MM:SS
    bool      show_file; // prepend file:line
    FILE     *file;      // optional plain-text file sink
} Log_Config;

typedef struct {
    bool      color;
    Log_Level level;
    bool      show_time;
    bool      show_file;
    FILE     *file;

#ifdef LOG__USE_WINCRIT
    CRITICAL_SECTION cs;
    bool             cs_init;
#endif

} Log_Global;

#ifdef LOG_IMPLEMENTATION
Log_Global log__g = {0};
#else
extern Log_Global log__g;
#endif

#define log_init(...) log__init((Log_Config){__VA_ARGS__})
LOGDEF void log__init(Log_Config cfg);

LOGDEF const char *log_seq(const char *s);
LOGDEF bool log_confirm(const char *fmt, ...) LOG_FMT(1, 2);
LOGDEF void log_log(Log_Level lvl, const char *file, int line, const char *fmt, ...) LOG_FMT(4, 5);
#define log_msg(...)   log_log(LOG_NPRE,  __FILE__, __LINE__, __VA_ARGS__)
#define log_debug(...) log_log(LOG_DEBUG, __FILE__, __LINE__, __VA_ARGS__)
#define log_info(...)  log_log(LOG_INFO,  __FILE__, __LINE__, __VA_ARGS__)
#define log_warn(...)  log_log(LOG_WARN,  __FILE__, __LINE__, __VA_ARGS__)
#define log_error(...) log_log(LOG_ERROR, __FILE__, __LINE__, __VA_ARGS__)
#define log_fatal(...) log_log(LOG_FATAL, __FILE__, __LINE__, __VA_ARGS__)
#define log_note(...)  log_log(LOG_NOTE,  __FILE__, __LINE__, __VA_ARGS__)
#define log_hint(...)  log_log(LOG_HINT,  __FILE__, __LINE__, __VA_ARGS__)

#ifdef LOG_IMPLEMENTATION

LOGDEF const char *log_seq(const char *s)
{
    return log__g.color ? s : "";
}

#ifdef LOG__USE_WINCRIT

static void log__lock(FILE *stream)
{
    (void)stream;
    if (log__g.cs_init) EnterCriticalSection(&log__g.cs);
}
static void log__unlock(FILE *stream)
{
    (void)stream;
    if (log__g.cs_init) LeaveCriticalSection(&log__g.cs);
}

#else // POSIX

static void log__lock(FILE *stream)   { flockfile(stream); }
static void log__unlock(FILE *stream) { funlockfile(stream); }

#endif

#if defined(_WIN32) || defined(_WIN64)
static bool log__enable_vt(void)
{
    HANDLE h = GetStdHandle(STD_OUTPUT_HANDLE);
    if (h == INVALID_HANDLE_VALUE) return false;
    DWORD mode = 0;
    if (!GetConsoleMode(h, &mode)) return false;
    return SetConsoleMode(h, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING) != 0;
}
#endif

static bool log__detect_color(void)
{
    if (getenv("NO_COLOR")) return false;

    const char *term = getenv("TERM");
    if (term && strcmp(term, "dumb") == 0) return false;

    if (!LOG__ISATTY(stdout)) return false;

#if defined(_WIN32) || defined(_WIN64)
    return log__enable_vt();
#else
    return true;
#endif
}

LOGDEF void log__init(Log_Config cfg)
{
    log__g.color     = cfg.use_color ? cfg.use_color : log__detect_color();
    log__g.level     = cfg.level;
    log__g.show_time = cfg.show_time;
    log__g.show_file = cfg.show_file;
    log__g.file      = cfg.file;

#ifdef LOG__USE_WINCRIT
    if (!log__g.cs_init) {
        InitializeCriticalSection(&log__g.cs);
        log__g.cs_init = true;
    }
#endif
}

static const char *log__level_str(Log_Level l)
{
    switch (l) {
        case LOG_NPRE:  return "";
        case LOG_DEBUG: return "debug:";
        case LOG_INFO:  return "info:";
        case LOG_WARN:  return "warn:";
        case LOG_ERROR: return "error:";
        case LOG_FATAL: return "fatal:";
        case LOG_NOTE:  return "note:";
        case LOG_HINT:  return "hint:";
    }
    return "?:";
}

static const char *log__level_color(Log_Level l)
{
#ifdef LOG_NO_COLOR
    (void)l;
    return "";
#else
    if (!log__g.color) return "";
    switch (l) {
        case LOG_NPRE:  return "";
        case LOG_DEBUG: return A_DIM;
        case LOG_INFO:  return A_BOLD_BLUE;
        case LOG_WARN:  return A_BOLD_MAGENTA;
        case LOG_ERROR: return A_BOLD_RED;
        case LOG_FATAL: return A_BOLD_BRED;
        case LOG_NOTE:  return A_BOLD_CYAN;
        case LOG_HINT:  return A_BOLD_GREEN;
    }
    return "";
#endif
}

static void log__time(char *buf, size_t sz)
{
    time_t     t  = time(NULL);
    struct tm *tm = localtime(&t);
    strftime(buf, sz, "%H:%M:%S", tm);
}

static void log__strip_ansi(char *dst, size_t dsz, const char *src)
{
    size_t di = 0;
    for (size_t si = 0; src[si] && di + 1 < dsz; ++si) {
        if (src[si] == '\x1b' && src[si + 1] == '[') {
            si += 2;
            while (src[si] && !(src[si] >= 'A' && src[si] <= 'Z') &&
                               !(src[si] >= 'a' && src[si] <= 'z')) {
                ++si;
            }
            continue;
        }
        dst[di++] = src[si];
    }
    dst[di] = '\0';
}

static void log__write(FILE *out, const char *msg)
{
    log__lock(out);
    fputs(msg, out);
    log__unlock(out);

    if (log__g.file) {
        size_t len   = strlen(msg);
        char  *plain = (char *)malloc(len + 1);
        if (plain) {
            log__strip_ansi(plain, len + 1, msg);
            log__lock(log__g.file);
            fputs(plain, log__g.file);
            log__unlock(log__g.file);
            free(plain);
        }
    }
}

static void log__vlog(Log_Level lvl, const char *file, int line, const char *fmt, va_list ap)
{
    if (lvl < log__g.level) return;

    char   buf[2048];
    size_t cap = sizeof(buf);
    int    off = 0;

#define LOG__APPEND(...) \
    do { \
        int _r = snprintf(buf + off, cap - (size_t)off, __VA_ARGS__); \
        if (_r > 0) off += (_r < (int)(cap - (size_t)off)) ? _r : (int)(cap - (size_t)off) - 1; \
    } while (0)

    if (log__g.show_time) {
        char t[32];
        log__time(t, sizeof(t));
        LOG__APPEND("%s[%s]%s ", log_seq(A_DIM_WHITE), t, log_seq(A_RESET));
    }

    if (lvl != LOG_NPRE) {
        LOG__APPEND("%s%s%s ",
            log_seq(log__level_color(lvl)),
            log__level_str(lvl),
            log_seq(A_RESET));
    }

    if (log__g.show_file && file) {
        LOG__APPEND("%s%s:%d:%s ",
            log_seq(A_DIM_WHITE), file, line, log_seq(A_RESET));
    }

    {
        va_list ap2;
        va_copy(ap2, ap);
        int r = vsnprintf(buf + off, cap - (size_t)off, fmt, ap2);
        va_end(ap2);
        if (r > 0) off += (r < (int)(cap - (size_t)off)) ? r : (int)(cap - (size_t)off) - 1;
    }

#undef LOG__APPEND

    if (off > (int)cap - 2) off = (int)cap - 2;
    buf[off++] = '\n';
    buf[off]   = '\0';

    FILE *out = (lvl >= LOG_ERROR) ? stderr : stdout;
    log__write(out, buf);
}

LOGDEF void log_log(Log_Level lvl, const char *file, int line, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    log__vlog(lvl, file, line, fmt, ap);
    va_end(ap);

    if (lvl == LOG_FATAL) exit(1);
}

LOGDEF bool log_confirm(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vprintf(fmt, ap);
    va_end(ap);

    printf(" %s[y/N]%s ", log_seq(A_BOLD_WHITE), log_seq(A_RESET));
    fflush(stdout);

    char buf[16];
    if (!fgets(buf, (int)sizeof(buf), stdin)) return false;
    return buf[0] == 'y' || buf[0] == 'Y';
}

#endif // LOG_IMPLEMENTATION

#endif // LOG_H_

/*
   ------------------------------------------------------------------------------
   This software is available under 2 licenses -- choose whichever you prefer.
   ------------------------------------------------------------------------------
   ALTERNATIVE A - MIT License
   Copyright (c) 2026 Rama Maulana (rhmvl)
   Permission is hereby granted, free of charge, to any person obtaining a copy of
   this software and associated documentation files (the "Software"), to deal in
   the Software without restriction, including without limitation the rights to
   use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
   of the Software, and to permit persons to whom the Software is furnished to do
   so, subject to the following conditions:
   The above copyright notice and this permission notice shall be included in all
   copies or substantial portions of the Software.
   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
   IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
   FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
   AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
   LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
   OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
   SOFTWARE.
   ------------------------------------------------------------------------------
   ALTERNATIVE B - Public Domain (www.unlicense.org)
   This is free and unencumbered software released into the public domain.
   Anyone is free to copy, modify, publish, use, compile, sell, or distribute this
   software, either in source code form or as a compiled binary, for any purpose,
   commercial or non-commercial, and by any means.
   In jurisdictions that recognize copyright laws, the author or authors of this
   software dedicate any and all copyright interest in the software to the public
   domain. We make this dedication for the benefit of the public at large and to
   the detriment of our heirs and successors. We intend this dedication to be an
   overt act of relinquishment in perpetuity of all present and future rights to
   this software under copyright law.
   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
   IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
   FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
   AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
   ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
   WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
   ------------------------------------------------------------------------------
*/
