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
  
   Macro Interface
     LOG_NO_COLOR      — strip all ANSI sequences at compile time
     LOG_GLOBAL        — share log__g across TUs (see above)
     LOG_IMPL_GLOBAL   — provide the definition of log__g (one TU only)
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
#    define A__RESET        ""
#    define A__BOLD         ""
#    define A__DIM          ""
#    define A__ITALIC       ""
#    define A__UNDERLINE    ""
#    define A__RED          ""
#    define A__GREEN        ""
#    define A__YELLOW       ""
#    define A__BLUE         ""
#    define A__MAGENTA      ""
#    define A__CYAN         ""
#    define A__WHITE        ""
#    define A__BRED         ""
#    define A__BGREEN       ""
#    define A__BYELLOW      ""
#    define A__BBLUE        ""
#    define A__BCYAN        ""
#    define A__BOLD_RED     ""
#    define A__BOLD_BRED    ""
#    define A__BOLD_GREEN   ""
#    define A__BOLD_YELLOW  ""
#    define A__BOLD_BLUE    ""
#    define A__BOLD_MAGENTA ""
#    define A__BOLD_CYAN    ""
#    define A__BOLD_WHITE   ""
#    define A__DIM_WHITE    ""
#else
#    define A__CSI "\x1b["
#    define A__RESET        A__CSI "0m"
#    define A__BOLD         A__CSI "1m"
#    define A__DIM          A__CSI "2m"
#    define A__ITALIC       A__CSI "3m"
#    define A__UNDERLINE    A__CSI "4m"
#    define A__RED          A__CSI "31m"
#    define A__GREEN        A__CSI "32m"
#    define A__YELLOW       A__CSI "33m"
#    define A__BLUE         A__CSI "34m"
#    define A__MAGENTA      A__CSI "35m"
#    define A__CYAN         A__CSI "36m"
#    define A__WHITE        A__CSI "37m"
#    define A__BRED         A__CSI "91m"
#    define A__BGREEN       A__CSI "92m"
#    define A__BYELLOW      A__CSI "93m"
#    define A__BBLUE        A__CSI "94m"
#    define A__BCYAN        A__CSI "96m"
#    define A__BOLD_RED     A__BOLD A__RED
#    define A__BOLD_BRED    A__BOLD A__BRED
#    define A__BOLD_GREEN   A__BOLD A__GREEN
#    define A__BOLD_YELLOW  A__BOLD A__YELLOW
#    define A__BOLD_BLUE    A__BOLD A__BLUE
#    define A__BOLD_MAGENTA A__BOLD A__MAGENTA
#    define A__BOLD_CYAN    A__BOLD A__CYAN
#    define A__BOLD_WHITE   A__BOLD A__WHITE
#    define A__DIM_WHITE    A__DIM  A__WHITE
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

#ifndef LOG_GLOBAL
static Log_Global log__g = {0};
#else
#  ifdef LOG_IMPLEMENTATION
Log_Global log__g = {0};
#  else
extern Log_Global log__g;
#  endif
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
        case LOG_DEBUG: return A__DIM;
        case LOG_INFO:  return A__BOLD_BLUE;
        case LOG_WARN:  return A__BOLD_MAGENTA;
        case LOG_ERROR: return A__BOLD_RED;
        case LOG_FATAL: return A__BOLD_BRED;
        case LOG_NOTE:  return A__BOLD_CYAN;
        case LOG_HINT:  return A__BOLD_GREEN;
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
        LOG__APPEND("%s[%s]%s ", log_seq(A__DIM_WHITE), t, log_seq(A__RESET));
    }

    if (lvl != LOG_NPRE) {
        LOG__APPEND("%s%s%s ",
            log_seq(log__level_color(lvl)),
            log__level_str(lvl),
            log_seq(A__RESET));
    }

    if (log__g.show_file && file) {
        LOG__APPEND("%s%s:%d:%s ",
            log_seq(A__DIM_WHITE), file, line, log_seq(A__RESET));
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

    printf(" %s[y/N]%s ", log_seq(A__BOLD_WHITE), log_seq(A__RESET));
    fflush(stdout);

    char buf[16];
    if (!fgets(buf, (int)sizeof(buf), stdin)) return false;
    return buf[0] == 'y' || buf[0] == 'Y';
}

#endif // LOG_IMPLEMENTATION

// public color-sequence macros
// These honor the runtime color flag via log_seq().
#define A_CSI        log_seq(A__CSI)

#define A_RESET      log_seq(A__RESET)
#define A_BOLD       log_seq(A__BOLD)
#define A_DIM        log_seq(A__DIM)
#define A_ITALIC     log_seq(A__ITALIC)
#define A_UNDERLINE  log_seq(A__UNDERLINE)

#define A_RED        log_seq(A__RED)
#define A_GREEN      log_seq(A__GREEN)
#define A_YELLOW     log_seq(A__YELLOW)
#define A_BLUE       log_seq(A__BLUE)
#define A_MAGENTA    log_seq(A__MAGENTA)
#define A_CYAN       log_seq(A__CYAN)
#define A_WHITE      log_seq(A__WHITE)

#define A_BRED       log_seq(A__BRED)
#define A_BGREEN     log_seq(A__BGREEN)
#define A_BYELLOW    log_seq(A__BYELLOW)
#define A_BBLUE      log_seq(A__BBLUE)
#define A_BCYAN      log_seq(A__BCYAN)

#define A_BOLD_RED     log_seq(A__BOLD_RED)
#define A_BOLD_BRED    log_seq(A__BOLD_BRED)
#define A_BOLD_GREEN   log_seq(A__BOLD_GREEN)
#define A_BOLD_YELLOW  log_seq(A__BOLD_YELLOW)
#define A_BOLD_BLUE    log_seq(A__BOLD_BLUE)
#define A_BOLD_MAGENTA log_seq(A__BOLD_MAGENTA)
#define A_BOLD_CYAN    log_seq(A__BOLD_CYAN)
#define A_BOLD_WHITE   log_seq(A__BOLD_WHITE)
#define A_DIM_WHITE    log_seq(A__DIM_WHITE)

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
