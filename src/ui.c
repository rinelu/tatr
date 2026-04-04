#include "ui.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <stdlib.h>

#if defined(_WIN32) || defined(_WIN64)
#  define UI_WINDOWS 1
#  include <windows.h>
#  include <io.h>
#  define isatty _isatty
#  define fileno _fileno
#else
#  define UI_POSIX 1
#  include <unistd.h>
#  include <sys/ioctl.h>
#endif

static struct {
    bool color;
    bool show_header;
} g = {0};

#ifdef UI_WINDOWS
static bool win_enable_vt(void)
{
    bool ok = true;
    HANDLE handles[2] = {
        GetStdHandle(STD_OUTPUT_HANDLE),
        GetStdHandle(STD_ERROR_HANDLE)
    };
    for (int i = 0; i < 2; i++) {
        if (handles[i] == INVALID_HANDLE_VALUE) { ok = false; continue; }
        DWORD mode = 0;
        if (!GetConsoleMode(handles[i], &mode)) { ok = false; continue; }
        if (!SetConsoleMode(handles[i], mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING)) ok = false;
    }
    return ok;
}
#endif

static bool detect_color(void)
{
    if (getenv("NO_COLOR")) return false;
    const char *term = getenv("TERM");
    if (term && strcmp(term, "dumb") == 0) return false;
    if (!isatty(fileno(stdout))) return false;
#ifdef UI_WINDOWS
    return win_enable_vt();
#else
    return true;
#endif
}

static int term_width(void)
{
#ifdef UI_WINDOWS
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    if (GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &csbi))
        return (int)(csbi.srWindow.Right - csbi.srWindow.Left + 1);
#elif defined(UI_POSIX)
    struct winsize ws;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0 && ws.ws_col > 0)
        return (int)ws.ws_col;
#endif
    const char *cols = getenv("COLUMNS");
    if (cols) {
        int w = atoi(cols);
        if (w > 0) return w;
    }
    return 80;
}

void ui__init(UIConfig cfg)
{
    g.show_header = cfg.show_header;
    g.color       = cfg.use_color ? cfg.use_color : detect_color();
}

static inline const char *seq(const char *s)
{
    return g.color ? s : "";
}

#define SV_FMT_COLOR(col) "%s%.*s%s"
#define SV_ARG_COLOR(col, sv) seq(col), (int)(sv).count, (sv).data, seq(A_RESET)

const char *ui_reset(void)
{
    return seq(A_RESET);
}

const char *ui_status_color(String_View status)
{
    if (!g.color) return "";
    if (sv_eq(status, sv_from_cstr("open")))        return A_BOLD_GREEN;
    if (sv_eq(status, sv_from_cstr("closed")))      return A_FG_CYAN;
    if (sv_eq(status, sv_from_cstr("wontfix")))     return A_DIM;
    if (sv_eq(status, sv_from_cstr("in-progress"))) return A_FG_BYELLOW;
    return "";
}

const char *ui_priority_color(String_View p)
{
    if (!g.color) return "";
    if (sv_eq(p, sv_from_cstr("critical"))) return A_BOLD_BRED;
    if (sv_eq(p, sv_from_cstr("high")))     return A_FG_RED;
    if (sv_eq(p, sv_from_cstr("normal")))   return A_FG_YELLOW;
    if (sv_eq(p, sv_from_cstr("low")))      return A_DIM_WHITE;
    return "";
}

static void sv_to_buf(String_View in, char *out, size_t outsz)
{
    if (outsz == 0) return;
    if (in.count < outsz) {
        memcpy(out, in.data, in.count);
        out[in.count] = '\0';
        return;
    }
    size_t clip = outsz - 4;
    memcpy(out, in.data, clip);
    memcpy(out + clip, "\xe2\x80\xa6", 4);
}

typedef struct {
    FILE       *out;
    const char *color;
    const char *label;
} MsgSpec;

static void vmsg(MsgSpec s, const char *fmt, va_list ap)
{
    if (s.label && s.label[0])
        fprintf(s.out, "%s%s:%s ", seq(s.color), s.label, seq(A_RESET));
    vfprintf(s.out, fmt, ap);
    fputc('\n', s.out);
}

#define DEFINE_MSG_FN(name, spec) \
void name(const char *fmt, ...) \
{ \
    va_list ap; \
    va_start(ap, fmt); \
    vmsg((spec), fmt, ap); \
    va_end(ap); \
}

DEFINE_MSG_FN(ui_msg,   ((MsgSpec){ stdout, "",               ""        }))
DEFINE_MSG_FN(ui_info,  ((MsgSpec){ stdout, A_BOLD_BLUE,      "note"    }))
DEFINE_MSG_FN(ui_warn,  ((MsgSpec){ stderr, A_BOLD_MAGENTA,   "warning" }))
DEFINE_MSG_FN(ui_error, ((MsgSpec){ stderr, A_BOLD_RED,       "error"   }))
DEFINE_MSG_FN(ui_fatal, ((MsgSpec){ stderr, A_BOLD_BRED,      "fatal"   }))
DEFINE_MSG_FN(ui_hint,  ((MsgSpec){ stdout, A_BOLD_CYAN,      "hint"    }))

#undef DEFINE_MSG_FN

bool ui_confirm(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vprintf(fmt, ap);
    va_end(ap);

    printf(" %s[y/N]%s ", seq(A_BOLD_WHITE), seq(A_RESET));
    fflush(stdout);

    char buf[16];
    if (!fgets(buf, sizeof(buf), stdin)) return false;
    return buf[0] == 'y' || buf[0] == 'Y';
}

#define COL_ID        24
#define COL_STATUS    13
#define COL_PRIORITY  10
#define COL_GAP       2

void ui_print_list_header(void)
{
    if (!g.show_header) return;

    ui_msg("%s%-*s  %-*s  %-*s  %s%s",
           seq(A_BOLD_WHITE),
           COL_ID,       "ID",
           COL_STATUS,   "STATUS",
           COL_PRIORITY, "PRIORITY",
           "TITLE",
           seq(A_RESET));
}

void ui_print_issue_row(const Issue *iss)
{
    int tw        = term_width();
    int max_title = tw - (COL_ID + COL_STATUS + COL_PRIORITY + 2 * COL_GAP);
    if (max_title < 8)  max_title = 8;
    if (max_title > 255) max_title = 255;

    char title[256];
    sv_to_buf(iss->title, title, (size_t)max_title + 1);

    printf("%s%-*.*s%s  ",
           seq(A_FG_BYELLOW), COL_ID, SV_Arg(iss->id), seq(A_RESET));

    printf("%s%-*.*s%s  ",
           seq(ui_status_color(iss->status)), COL_STATUS, SV_Arg(iss->status), seq(A_RESET));

    printf("%s%-*.*s%s  ",
           seq(ui_priority_color(iss->priority)), COL_PRIORITY, SV_Arg(iss->priority), seq(A_RESET));

    ui_msg("%s", title);
}

void ui_print_search_row(const Issue *iss)
{
    ui_msg("%s"SV_Fmt"%s  %s("SV_Fmt")%s  "SV_Fmt,
           seq(A_FG_BYELLOW), SV_Arg(iss->id), seq(A_RESET),
           seq(ui_status_color(iss->status)), SV_Arg(iss->status), seq(A_RESET),
           SV_Arg(iss->title));
}

static void print_rule(void)
{
    int w = term_width();
    if (w > 120) w = 120;

    fputs(seq(A_DIM), stdout);
    for (int i = 0; i < w; i++)
        fputs(g.color ? "\xe2\x94\x80" : "-", stdout);
    fputs(seq(A_RESET), stdout);
    fputc('\n', stdout);
}

void ui_print_issue_full(const Issue *iss)
{
    ui_msg("%sissue %s"SV_Fmt"%s", seq(A_FG_BYELLOW), seq(A_BOLD), SV_Arg(iss->id), seq(A_RESET));

#define LABEL(s) printf("%s%-10s%s", seq(A_BOLD_WHITE), (s), seq(A_RESET))

    LABEL("Author:");
    ui_msg("  "SV_Fmt, SV_Arg(iss->created));

    LABEL("Status:");
    ui_msg("  %s"SV_Fmt"%s", seq(ui_status_color(iss->status)), SV_Arg(iss->status), seq(A_RESET));

    LABEL("Priority:");
    ui_msg("  %s"SV_Fmt"%s", seq(ui_priority_color(iss->priority)), SV_Arg(iss->priority), seq(A_RESET));

    if (iss->tags.count)
        LABEL("Tags:"), ui_msg("  %s"SV_Fmt"%s", seq(A_FG_CYAN), SV_Arg(iss->tags), seq(A_RESET));

#undef LABEL

    ui_msg("\n    %s%.*s%s", seq(A_BOLD), (int)iss->title.count, iss->title.data, seq(A_RESET));

    if (iss->body.count) {
        String_View cursor = iss->body, line;
        while (cursor.count > 0) {
            line = sv_slice_by_delim(&cursor, '\n');
            String_View trimmed = sv_trim(line);
            if (sv_eq(trimmed, sv_from_cstr("---comment---"))) {
                print_rule();
                continue;
            }
            ui_msg("    "SV_Fmt, SV_Arg(line));
        }
    }

    fputc('\n', stdout);
}
