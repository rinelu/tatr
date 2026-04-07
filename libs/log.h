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
#        define LOG_FMT(STRING_INDEX, FIRST_TO_CHECK) __attribute__ ((format (__MINGW_PRINTF_FORMAT, STRING_INDEX, FIRST_TO_CHECK)))
#    else
#        define LOG_FMT(STRING_INDEX, FIRST_TO_CHECK) __attribute__ ((format (printf, STRING_INDEX, FIRST_TO_CHECK)))
#    endif // __MINGW_PRINTF_FORMAT
#endif

#if defined(_WIN32) || defined(_WIN64)
#    include <windows.h>
#    include <io.h>
#    define isatty _isatty
#    define fileno _fileno
#else
#    include <unistd.h>
#endif

#define A__CSI "\x1b["

#ifdef LOG_NO_COLOR
#    define A__RESET     "" 
#    define A__BOLD      "" 
#    define A__DIM       "" 
#    define A__ITALIC    "" 
#    define A__UNDERLINE "" 
#    define A__RED       "" 
#    define A__GREEN     "" 
#    define A__YELLOW    "" 
#    define A__BLUE      "" 
#    define A__MAGENTA   "" 
#    define A__CYAN      "" 
#    define A__WHITE     "" 
#    define A__BRED      "" 
#    define A__BGREEN    "" 
#    define A__BYELLOW   "" 
#    define A__BBLUE     "" 
#    define A__BCYAN     "" 
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
#    define A__RESET     A__CSI "0m"
#    define A__BOLD      A__CSI "1m"
#    define A__DIM       A__CSI "2m"
#    define A__ITALIC    A__CSI "3m"
#    define A__UNDERLINE A__CSI "4m"

#    define A__RED       A__CSI "31m"
#    define A__GREEN     A__CSI "32m"
#    define A__YELLOW    A__CSI "33m"
#    define A__BLUE      A__CSI "34m"
#    define A__MAGENTA   A__CSI "35m"
#    define A__CYAN      A__CSI "36m"
#    define A__WHITE     A__CSI "37m"

#    define A__BRED      A__CSI "91m"
#    define A__BGREEN    A__CSI "92m"
#    define A__BYELLOW   A__CSI "93m"
#    define A__BBLUE     A__CSI "94m"
#    define A__BCYAN     A__CSI "96m"

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
    bool      use_color;
    Log_Level level;
    bool      show_time;
    bool      show_file;
    // optional file output
    FILE     *file;
} Log_Config;

typedef struct {
    bool color;
    Log_Level level;
    bool show_time;
    bool show_file;
    FILE *file;
} Log_Global;

#ifndef LOG_GLOBAL
static Log_Global log__g = {0};
#else
#    ifdef LOG_IMPL_GLOBAL
Log_Global log__g = {0};
#    else
extern Log_Global log__g;
#    endif
#endif

#if defined(_WIN32) || defined(_WIN64)
static bool log__enable_vt(void)
{
    HANDLE h = GetStdHandle(STD_OUTPUT_HANDLE);
    if (h == INVALID_HANDLE_VALUE) return false;

    DWORD mode = 0;
    if (!GetConsoleMode(h, &mode)) return false;

    return SetConsoleMode(h, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
}
#endif

static bool log__detect_color(void)
{
    if (getenv("NO_COLOR")) return false;

    const char *term = getenv("TERM");
    if (term && strcmp(term, "dumb") == 0) return false;

    if (!isatty(fileno(stdout))) return false;

#if defined(_WIN32) || defined(_WIN64)
    return log__enable_vt();
#else
    return true;
#endif
}

static inline const char *log_seq(const char *s)
{
    return log__g.color ? s : "";
}

#define log_init(...) log__init((Log_Config){__VA_ARGS__})

static inline void log__init(Log_Config cfg)
{
    log__g.color     = cfg.use_color ? cfg.use_color : log__detect_color();
    log__g.level     = cfg.level;
    log__g.show_time = cfg.show_time;
    log__g.show_file = cfg.show_file;
    log__g.file      = cfg.file;
}

static const char *log__level_str(Log_Level l)
{
    switch (l) {
        case LOG_NPRE: return "";
        case LOG_DEBUG: return "debug:";
        case LOG_INFO:  return "info:";
        case LOG_WARN:  return "warn:";
        case LOG_ERROR: return "error:";
        case LOG_FATAL: return "fatal:";
        case LOG_NOTE:  return "note:";
        case LOG_HINT:  return "hint:";
    }
    return "?";
}

static const char *log__level_color(Log_Level l)
{
    if (!log__g.color) return "";

    switch (l) {
        case LOG_NPRE: return "";
        case LOG_DEBUG: return A__DIM;
        case LOG_INFO:  return A__BOLD A__BLUE;
        case LOG_WARN:  return A__BOLD A__MAGENTA;
        case LOG_ERROR: return A__BOLD A__RED;
        case LOG_FATAL: return A__BOLD A__BRED;
        case LOG_NOTE:  return A__BOLD A__BRED;
        case LOG_HINT:  return A__BOLD A__BRED;
    }
    return "";
}

static void log__time(char *buf, size_t sz)
{
    time_t t = time(NULL);
    struct tm *tm = localtime(&t);
    strftime(buf, sz, "%H:%M:%S", tm);
}

static void log__write(FILE *out, const char *msg)
{
    flockfile(out);
    fputs(msg, out);
    funlockfile(out);

    if (log__g.file) {
        flockfile(log__g.file);
        fputs(msg, log__g.file);
        funlockfile(log__g.file);
    }
}

static void log__vlog(Log_Level lvl, const char *file, int line, const char *fmt, va_list ap)
{
    if (lvl < log__g.level) return;

    char buf[2048];
    int off = 0;

#define ul (unsigned long)
    if (log__g.show_time) {
        char t[32];
        log__time(t, sizeof(t));
        off += snprintf(buf + off, sizeof(buf) - ul off, "[%s] ", t);
    }

    off += snprintf(buf + off, sizeof(buf) - ul off,
        "%s%s%s ",
        log_seq(log__level_color(lvl)),
        log__level_str(lvl),
        log_seq(A__RESET));

    if (log__g.show_file) {
        off += snprintf(buf + off, sizeof(buf) - ul off, "%s:%d: ", file, line);
    }

    off += vsnprintf(buf + off, sizeof(buf) - ul off, fmt, ap);
#undef ul

    if (off < (int)sizeof(buf) - 2) {
        buf[off++] = '\n';
        buf[off] = 0;
    }

    FILE *out = (lvl >= LOG_ERROR) ? stderr : stdout;
    log__write(out, buf);
}

#define log_msg(...)   log__log(LOG_NPRE,  __FILE__, __LINE__, __VA_ARGS__)
#define log_debug(...) log__log(LOG_DEBUG, __FILE__, __LINE__, __VA_ARGS__)
#define log_info(...)  log__log(LOG_INFO,  __FILE__, __LINE__, __VA_ARGS__)
#define log_warn(...)  log__log(LOG_WARN,  __FILE__, __LINE__, __VA_ARGS__)
#define log_error(...) log__log(LOG_ERROR, __FILE__, __LINE__, __VA_ARGS__)
#define log_fatal(...) log__log(LOG_FATAL, __FILE__, __LINE__, __VA_ARGS__)

// static inline void log__log(Log_Level lvl, const char *file, int line, const char *fmt, ...) LOG_FMT(4, 5);
static inline void log__log(Log_Level lvl, const char *file, int line, const char *fmt, ...) 
{
    va_list ap;
    va_start(ap, fmt);
    log__vlog(lvl, file, line, fmt, ap);
    va_end(ap);

    if (lvl == LOG_FATAL) exit(1);
}

static inline bool log_confirm(const char *fmt, ...) LOG_FMT(1, 2);
static inline bool log_confirm(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vprintf(fmt, ap);
    va_end(ap);

    printf(" %s[y/N]%s ", log_seq(A__BOLD A__WHITE), log_seq(A__RESET));

    fflush(stdout);

    char buf[16];
    if (!fgets(buf, sizeof(buf), stdin))
        return false;

    return buf[0] == 'y' || buf[0] == 'Y';
}

#define A_CSI log_seq(A__CSI)

#define A_RESET     log_seq(A__RESET)
#define A_BOLD      log_seq(A__BOLD)
#define A_DIM       log_seq(A__DIM)
#define A_ITALIC    log_seq(A__ITALIC)
#define A_UNDERLINE log_seq(A__UNDERLINE)
       
#define A_RED       log_seq(A__RED)
#define A_GREEN     log_seq(A__GREEN)
#define A_YELLOW    log_seq(A__YELLOW)
#define A_BLUE      log_seq(A__BLUE)
#define A_MAGENTA   log_seq(A__MAGENTA)
#define A_CYAN      log_seq(A__CYAN)
#define A_WHITE     log_seq(A__WHITE)
      
#define A_BRED      log_seq(A__BRED)
#define A_BGREEN    log_seq(A__BGREEN)
#define A_BYELLOW   log_seq(A__BYELLOW)
#define A_BBLUE     log_seq(A__BBLUE)
#define A_BCYAN     log_seq(A__BCYAN)

#define A_BOLD_RED     A__RED
#define A_BOLD_BRED    A__BRED
#define A_BOLD_GREEN   A__GREEN
#define A_BOLD_YELLOW  A__YELLOW
#define A_BOLD_BLUE    A__BLUE
#define A_BOLD_MAGENTA A__MAGENTA
#define A_BOLD_CYAN    A__CYAN
#define A_BOLD_WHITE   A__WHITE
#define A_DIM_WHITE    A__WHITE

#endif // LOG_H_
