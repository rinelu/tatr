#ifndef UI_H_
#define UI_H_

#include <stdbool.h>
#include "issue.h"

#define CSI            "\x1b["
#define A_RESET        CSI "0m"
#define A_BOLD         CSI "1m"
#define A_DIM          CSI "2m"
#define A_ITALIC       CSI "3m"
#define A_UNDERLINE    CSI "4m"

#define A_FG_BLACK     CSI "30m"
#define A_FG_RED       CSI "31m"
#define A_FG_GREEN     CSI "32m"
#define A_FG_YELLOW    CSI "33m"
#define A_FG_BLUE      CSI "34m"
#define A_FG_MAGENTA   CSI "35m"
#define A_FG_CYAN      CSI "36m"
#define A_FG_WHITE     CSI "37m"
#define A_FG_DEFAULT   CSI "39m"

#define A_FG_BRED      CSI "91m"
#define A_FG_BGREEN    CSI "92m"
#define A_FG_BYELLOW   CSI "93m"
#define A_FG_BBLUE     CSI "94m"
#define A_FG_BCYAN     CSI "96m"

#define A_BOLD_RED     A_BOLD A_FG_RED
#define A_BOLD_BRED    A_BOLD A_FG_BRED
#define A_BOLD_GREEN   A_BOLD A_FG_GREEN
#define A_BOLD_YELLOW  A_BOLD A_FG_YELLOW
#define A_BOLD_BLUE    A_BOLD A_FG_BLUE
#define A_BOLD_MAGENTA A_BOLD A_FG_MAGENTA
#define A_BOLD_CYAN    A_BOLD A_FG_CYAN
#define A_BOLD_WHITE   A_BOLD A_FG_WHITE
#define A_DIM_WHITE    A_DIM  A_FG_WHITE

typedef struct {
    bool use_color;
    bool show_header;
} UIConfig;

#define ui_init(...) ui__init((UIConfig){__VA_ARGS__})
void ui__init(UIConfig cfg);

void ui_print_list_header(void);
void ui_print_issue_row(const Issue *iss);
void ui_print_issue_full(const Issue *iss);
void ui_print_search_row(const Issue *iss);

void ui_msg(const char *fmt, ...);
void ui_info(const char *fmt, ...);
void ui_warn(const char *fmt, ...);
void ui_error(const char *fmt, ...);
bool ui_confirm(const char *fmt, ...);

const char *ui_status_color(String_View status);
const char *ui_priority_color(String_View priority);

#endif // UI_H_
