#include "ui.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#if defined(_WIN32) || defined(_WIN64)
#  include <windows.h>
#else
#  include <unistd.h>
#  include <sys/ioctl.h>
#endif

static struct {
    bool show_header;
} g = {0};

void ui__init(UIConfig cfg)
{
    log_init(.use_color=false);
    g.show_header = cfg.show_header;
}

static int term_width(void)
{
#if defined(_WIN32)
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    if (GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &csbi))
        return (int)(csbi.srWindow.Right - csbi.srWindow.Left + 1);
#else
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

const char *ui_status_color(String_View status)
{
    if (sv_eq(status, sv_from_cstr("open")))        return A_BOLD_GREEN;
    if (sv_eq(status, sv_from_cstr("closed")))      return A_CYAN;
    if (sv_eq(status, sv_from_cstr("wontfix")))     return A_DIM;
    if (sv_eq(status, sv_from_cstr("in-progress"))) return A_BYELLOW;
    return "";
}

const char *ui_priority_color(String_View p)
{
    if (sv_eq(p, sv_from_cstr("critical"))) return A_BOLD_BRED;
    if (sv_eq(p, sv_from_cstr("high")))     return A_RED;
    if (sv_eq(p, sv_from_cstr("normal")))   return A_YELLOW;
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

#define COL_ID        24
#define COL_STATUS    13
#define COL_PRIORITY  10
#define COL_GAP       2

void ui_print_list_header(void)
{
    if (!g.show_header) return;

    log_msg("%s%-*s  %-*s  %-*s  %s%s",
        log_seq(A_BOLD_WHITE),
        COL_ID, "ID",
        COL_STATUS, "STATUS",
        COL_PRIORITY, "PRIORITY",
        "TITLE",
        log_seq(A_RESET));
}

void ui_print_issue_row(const Issue *iss)
{
    int tw = term_width();
    int max_title = tw - (COL_ID + COL_STATUS + COL_PRIORITY + 2 * COL_GAP);

    if (max_title < 8) max_title = 8;
    if (max_title > 255) max_title = 255;

    char title[256];
    sv_to_buf(iss->title, title, (size_t)max_title + 1);

    printf("%s%-*.*s%s  ",
        log_seq(A_BYELLOW), COL_ID, SV_Arg(iss->id), log_seq(A_RESET));

    printf("%s%-*.*s%s  ",
        log_seq(ui_status_color(iss->status)), COL_STATUS, SV_Arg(iss->status), log_seq(A_RESET));

    printf("%s%-*.*s%s  ",
        log_seq(ui_priority_color(iss->priority)), COL_PRIORITY, SV_Arg(iss->priority), log_seq(A_RESET));

    log_msg("%s", title);
}

void ui_print_search_row(const Issue *iss)
{
    log_msg("%s"SV_Fmt"%s  %s("SV_Fmt")%s  "SV_Fmt,
        log_seq(A_BYELLOW), SV_Arg(iss->id), log_seq(A_RESET),
        log_seq(ui_status_color(iss->status)),
        SV_Arg(iss->status), log_seq(A_RESET),
        SV_Arg(iss->title));
}

static void print_rule(void)
{
    int w = term_width();
    if (w > 120) w = 120;

    fputs(log_seq(A_DIM), stdout);
    for (int i = 0; i < w; i++)
        fputs(log__g.color ? "\xe2\x94\x80" : "-", stdout);
    fputs(log_seq(A_RESET), stdout);
    fputc('\n', stdout);
}

void ui_print_issue_full(const Issue *iss)
{
    log_msg("%sissue %s"SV_Fmt"%s", log_seq(A_BYELLOW), log_seq(A_BOLD), SV_Arg(iss->id), log_seq(A_RESET));

#define LABEL(s) printf("%s%-10s%s", log_seq(A_BOLD_WHITE), (s), log_seq(A_RESET))

    LABEL("Author:");
    log_msg("  "SV_Fmt, SV_Arg(iss->created));

    LABEL("Status:");
    log_msg("  %s"SV_Fmt"%s",
        log_seq(ui_status_color(iss->status)), SV_Arg(iss->status), log_seq(A_RESET));

    LABEL("Priority:");
    log_msg("  %s"SV_Fmt"%s", log_seq(ui_priority_color(iss->priority)), SV_Arg(iss->priority), log_seq(A_RESET));

    if (iss->tags.count)
        LABEL("Tags:"),
        log_msg("  %s"SV_Fmt"%s", log_seq(A_CYAN), SV_Arg(iss->tags), log_seq(A_RESET));

#undef LABEL

    log_msg("\n    %s%.*s%s", log_seq(A_BOLD), (int)iss->title.count, iss->title.data, log_seq(A_RESET));

    if (iss->body.count) {
        String_View cursor = iss->body, line;

        while (cursor.count > 0) {
            line = sv_slice_by_delim(&cursor, '\n');

            String_View trimmed = sv_trim(line);
            if (sv_eq(trimmed, sv_from_cstr("---comment---"))) {
                print_rule();
                continue;
            }

            log_msg("    "SV_Fmt, SV_Arg(line));
        }
    }

    fputc('\n', stdout);
}
