#ifndef UI_H_
#define UI_H_

#include "astring.h"
#include "tatrlog.h"
#include <stdbool.h>

int term_width(void);
void print_rule(void);
const char *ui_status_color(String_View status);
const char *ui_priority_color(String_View priority);
const char *ui_event_color(TatrLog_Event e);
void ui_format_time_relative(time_t t, char *buf, size_t sz);
void ui_wrap( FILE *s, const char *text, int indent_first, int indent_rest, int width);

#endif // UI_H_
