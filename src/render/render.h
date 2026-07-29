#ifndef TATR_RENDER_H_
#define TATR_RENDER_H_

#include <stdio.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>
#include <time.h>

#include "issue.h"
#include "error.h"
#include "config.h"
#include "tatrlog.h"

typedef enum {
    TATR_FMT_HUMAN = 0,
    TATR_FMT_JSON,
} Tatr_Format;

// ---- Result: one row, as used by `tatr list` ----

typedef struct {
    String_View id;
    String_View title;
    Issue_Status_Kind status;
    Issue_Priority_Kind priority;
} Tatr_Issue_Summary;

typedef struct {
    Tatr_Issue_Summary *items;
    size_t count;
    size_t capacity;
} Tatr_Issue_Summary_Array;

typedef struct {
    bool show_header;
    Tatr_Issue_Summary_Array issues;
} Tatr_Issue_List_Result;

// ---- Result: single full issue, as used by `tatr show` ----

typedef struct {
    String_View id;
    String_View title;
    String_View created;
    String_View tags;
    Issue_Status_Kind status;
    Issue_Priority_Kind priority;
    String_View body; // already split from the header
                      // may contain "---comment---" section markers

    bool raw_mode;     // true => render the raw issue file verbatim
    String_View raw;

    bool has_attachments_dir;  // false => attachments/ doesn't exist at all 
    const char **attachments;  // may be NULL even if has_attachments_dir is true, when the directory is empty        
    size_t attachment_count;
} Tatr_Issue_View;

// ---- Result: `tatr attachls` ----

typedef struct {
    String_View issue_id;
    const char **items; // sorted attachment filenames, temp-allocated
    size_t count;
} Tatr_Attachment_List_Result;

// ---- Result: `tatr search` ----

typedef struct {
    Tatr_Issue_Summary_Array matches; // already limited, in display order
    const char *query;                // first query token, for the summary line
} Tatr_Search_Result;

// ---- Result: `tatr log` ----

typedef struct {
    bool oneline;
    const TatrLog_Entry *const *items; // temp array of pointers, display order, already limited
    size_t count;
} Tatr_Log_Result;

// ---- Result: `tatr status` ----

typedef struct {
    const char *id;
    String_View title;
    Issue_Status_Kind status;
    Issue_Priority_Kind priority;
    const char *updated_relative; // e.g. "3 days ago", temp-allocated
} Tatr_Status_Issue;

typedef struct {
    String_View tag;
    size_t count;
} Tatr_Status_Tag_Count;

typedef struct {
    size_t total, open, in_progress, closed;

    Tatr_Status_Issue *high_priority; size_t high_priority_count;
    Tatr_Status_Issue *stale;         size_t stale_count;
    Tatr_Status_Issue *recent;        size_t recent_count; // already limited
    Tatr_Status_Tag_Count *top_tags;  size_t top_tags_count; // already limited

    uint64_t stale_days;
} Tatr_Status_Result;

// ---- Result: `tatr config --list` / `--keys` ----

typedef struct {
    const char *key;
    const char *val;
    const char *source; // "local", "global", or "default"
} Tatr_Config_Resolved_Entry;

typedef struct {
    bool show_local;
    bool show_global;
    bool show_resolved;

    const Config_Store *local;  // NULL if !show_local
    const Config_Store *global; // NULL if !show_global

    Tatr_Config_Resolved_Entry *resolved; // NULL if !show_resolved
    size_t resolved_count;
} Tatr_Config_List_Result;

typedef struct {
    const Config_Key_Def *keys;
    size_t count;
} Tatr_Config_Keys_Result;

// ---- Renderer vtable ----

typedef struct {
    void (*issue_view)(FILE *out, const Tatr_Issue_View *v);
    void (*issue_list)(FILE *out, const Tatr_Issue_List_Result *r);
    void (*attachment_list)(FILE *out, const Tatr_Attachment_List_Result *r);
    void (*search_result)(FILE *out, const Tatr_Search_Result *r);
    void (*log_result)(FILE *out, const Tatr_Log_Result *r);
    void (*status)(FILE *out, const Tatr_Status_Result *r);
    void (*config_list)(FILE *out, const Tatr_Config_List_Result *r);
    void (*config_keys)(FILE *out, const Tatr_Config_Keys_Result *r);
    void (*message)(FILE *out, const char *msg);
    void (*error)(FILE *out, Tatr_Error err, const char *context);
} Renderer;

extern const Renderer RENDER_HUMAN;
extern const Renderer RENDER_JSON;

const Renderer *renderer_for(Tatr_Format fmt);

#endif // TATR_RENDER_H_
