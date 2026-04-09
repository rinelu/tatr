#ifndef UI_H_
#define UI_H_

#include <stdbool.h>
#include "issue.h"

#define LOG_GLOBAL
#include "log.h"

typedef struct {
    bool show_header;
} UIConfig;

#define ui_init(...) ui__init((UIConfig){__VA_ARGS__})
void ui__init(UIConfig cfg);

void ui_print_list_header(void);
void ui_print_issue_row(const Issue *iss);
void ui_print_issue_full(const Issue *iss);
void ui_print_search_row(const Issue *iss);

const char *ui_status_color(String_View status);
const char *ui_priority_color(String_View priority);

#endif // UI_H_
