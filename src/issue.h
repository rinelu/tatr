#ifndef ISSUE_H_
#define ISSUE_H_

#include <stdbool.h>
#include "astring.h"

typedef enum {
    ISSUE_SINVALID = -1,
    ISSUE_SOPEN = 0,
    ISSUE_SCLOSED,
    ISSUE_SWONTFIX,
    ISSUE_SONGOING
} Issue_StatusKind;

typedef enum {
    ISSUE_PINVALID = -1,
    ISSUE_PLOW = 0,
    ISSUE_PNORMAL,
    ISSUE_PHIGH,
    ISSUE_PCRITICAL,
    COUNT_ISSUE_PKIND,
} Issue_PriorityKind;

typedef struct {
    const char* path;
    const char* attach_path;
    const char* dpath;
    String_View id;

    String_View title;
    Issue_StatusKind status;
    Issue_PriorityKind priority;
    String_View tags;
    String_View created;

    String_View header; // before ---
    String_View body;   // after ---
    String_View raw;    // entire file
    String_Builder raw_sb;
} Issue;

bool issue_init_empty(Issue *iss);
bool issue_load(const char *id, Issue *out);
bool issue_save(Issue *iss);
void issue_free(Issue *iss);

Issue_StatusKind issue_status_from_cstr(const char *str);
Issue_StatusKind issue_status_from_sv(String_View sv);
const char *issue_status_to_cstr(Issue_StatusKind s);
#define issue_status_to_sv(s) sv_from_cstr(issue_status_to_cstr(s))

Issue_PriorityKind issue_priority_from_cstr(const char *str);
Issue_PriorityKind issue_priority_from_sv(String_View sv);
const char *issue_priority_to_cstr(Issue_PriorityKind p);
#define issue_priority_to_sv(p) sv_from_cstr(issue_priority_to_cstr(p))

bool issue_get_field(Issue *iss, const char *key, String_View *out);
bool issue_replace_field(Issue *iss, const char *field, const char *value);
bool issue_set_status(Issue *iss, Issue_StatusKind s);
bool issue_set_priority(Issue *iss, Issue_PriorityKind p);

bool issue_is_closed(const Issue *iss);
bool issue_has_tag(const Issue *iss, const char *tag);

#endif // ISSUE_H_
