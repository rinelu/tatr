#ifndef ISSUE_H_
#define ISSUE_H_

#include <stdbool.h>
#include "astring.h"
#include "backend.h"
#include "fs.h"

typedef enum {
    ISSUE_SINVALID = -1,
    ISSUE_SOPEN = 0,
    ISSUE_SCLOSED,
    ISSUE_SWONTFIX,
    ISSUE_SONGOING
} Issue_Status_Kind;

typedef enum {
    ISSUE_PINVALID = -1,
    ISSUE_PLOW = 0,
    ISSUE_PNORMAL,
    ISSUE_PHIGH,
    ISSUE_PCRITICAL,
    COUNT_ISSUE_PKIND,
} Issue_Priority_Kind;

typedef struct {
    const char* attach_path;
    String_View id;

    String_View title;
    Issue_Status_Kind status;
    Issue_Priority_Kind priority;
    String_View tags;
    String_View created;

    String_View header; // before ---
    String_View body;   // after ---
    String_View raw;    // entire file
    String_Builder raw_sb;
} Issue;

// Storage backend used by all functions below. Defaults to storage_backend_flatfile()
// Its overridable so libtatr can run against a fake/in-memory backend without a real temp directory.
void             issue_set_backend(Storage_Backend *be);
Storage_Backend *issue_backend(void);

bool issue_init_empty(Issue *iss);
bool issue_load(const char *id, Issue *out);
bool issue_save(Issue *iss);
void issue_free(Issue *iss);

// Reserve storage for a new issue `id` and populate `out` as an
// empty issue ready to have fields set and then issue_save().
// false on conflict (id already exists) or I/O error.
bool issue_create(const char *id, Issue *out);

bool issue_delete(const char *id);
bool issue_exists(const char *id);
bool issue_list_ids(File_Paths *out);

Issue_Status_Kind issue_status_from_cstr(const char *str);
Issue_Status_Kind issue_status_from_sv(String_View sv);
const char *issue_status_to_cstr(Issue_Status_Kind s);
#define issue_status_to_sv(s) sv_from_cstr(issue_status_to_cstr(s))

Issue_Priority_Kind issue_priority_from_cstr(const char *str);
Issue_Priority_Kind issue_priority_from_sv(String_View sv);
const char *issue_priority_to_cstr(Issue_Priority_Kind p);
#define issue_priority_to_sv(p) sv_from_cstr(issue_priority_to_cstr(p))

bool issue_get_field(Issue *iss, const char *key, String_View *out);
bool issue_replace_field(Issue *iss, const char *field, const char *value);
bool issue_set_status(Issue *iss, Issue_Status_Kind s);
bool issue_set_priority(Issue *iss, Issue_Priority_Kind p);

bool issue_is_closed(const Issue *iss);
bool issue_has_tag(const Issue *iss, const char *tag);

#endif // ISSUE_H_
