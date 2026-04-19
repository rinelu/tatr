#include "ui.h"
#include "log.h"

#include <stdio.h>
#include <stdlib.h>

#if defined(_WIN32) || defined(_WIN64)
#  include <windows.h>
#else
#  include <unistd.h>
#  include <sys/ioctl.h>
#endif

int term_width(void)
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

const char *ui_event_color(TatrLog_Event e)
{
    switch (e) {
    case TATRLOG_CREATE:  return A_BOLD_GREEN;
    case TATRLOG_DELETE:  return A_BOLD_RED;
    case TATRLOG_CLOSE:   return A_DIM;
    case TATRLOG_REOPEN:  return A_BOLD_CYAN;
    case TATRLOG_EDIT:    return A_BYELLOW;
    case TATRLOG_TAG:     return A_CYAN;
    case TATRLOG_COMMENT: return A_BLUE;
    case TATRLOG_ATTACH:  return A_MAGENTA;
    case TATRLOG_DETACH:  return A_MAGENTA;
    default:              return "";
    }
}

void ui_format_time_relative(time_t t, char *buf, size_t sz)
{
    time_t now  = time(NULL);
    double diff = difftime(now, t);

    if (diff < 60)
        snprintf(buf, sz, "just now");
    else if (diff < 3600)
        snprintf(buf, sz, "%.0fm ago", diff / 60.0);
    else if (diff < 86400)
        snprintf(buf, sz, "%.0fh ago", diff / 3600.0);
    else if (diff < 86400 * 30)
        snprintf(buf, sz, "%.0fd ago", diff / 86400.0);
    else {
        struct tm *tm = localtime(&t);
        strftime(buf, sz, "%Y-%m-%d", tm);
    }
}

void print_rule(void)
{
    int w = term_width();
    if (w > 120) w = 120;

    fputs(log_seq(A_DIM), stdout);
    for (int i = 0; i < w; i++)
        fputs(log__g.color ? "\xe2\x94\x80" : "-", stdout);
    fputs(log_seq(A_RESET), stdout);
    fputc('\n', stdout);
}

void ui_wrap(FILE *s, const char *text, int indent_first, int indent_rest, int width)
{
    const char *p = text;
    int line = 0;

    while (*p) {
        if (line != 0) {
            fprintf(s, "%*s", indent_rest, "");
        }
        // Word-wrap one line worth of text
        int avail = width - (line == 0 ? indent_first : indent_rest);
        if (avail < 1) avail = 1;

        int i = 0;
        int last_space = -1;
        while (p[i] && p[i] != '\n' && i < avail) {
            if (p[i] == ' ') last_space = i;
            i++;
        }

        int cut = i;
        if (p[i] && p[i] != '\n' && last_space > 0)
            cut = last_space;

        fwrite(p, 1, (size_t)cut, s);
        fputc('\n', s);

        p += cut;
        while (*p == ' ') p++;
        if (*p == '\n') p++;

        line++;
    }
}
