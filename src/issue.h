#ifndef ISSUE_H_
#define ISSUE_H_

#include <stdbool.h>
#include "astring.h"

typedef struct {
    const char* path;
    const char* attach_path;
    const char* dpath;
    String_View id;

    String_View title;
    String_View status;
    String_View priority;
    String_View tags;
    String_View created;

    String_View header; // before ---
    String_View body;   // after ---
    String_View raw;    // entire file
    String_Builder raw_sb;
} Issue;

bool issue_load(const char *id, Issue *out);
bool issue_save(Issue *iss);
void issue_free(Issue *iss);

bool issue_get_field(Issue *iss, const char *key, String_View *out);
bool issue_replace_field(Issue *iss, const char *field, const char *value);

bool issue_is_closed(const Issue *iss);
bool issue_has_tag(const Issue *iss, const char *tag);

#endif // ISSUE_H_
