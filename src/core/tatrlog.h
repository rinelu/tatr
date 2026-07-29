#ifndef TATRLOG_H_
#define TATRLOG_H_

#include "astring.h"

#include <stdbool.h>
#include <stddef.h>
#include <time.h>

typedef enum {
    TATRLOG_CREATE = 0,
    TATRLOG_EDIT,
    TATRLOG_CLOSE,
    TATRLOG_REOPEN,
    TATRLOG_DELETE,
    TATRLOG_TAG,
    TATRLOG_COMMENT,
    TATRLOG_ATTACH,
    TATRLOG_DETACH,
    COUNT_TATRLOG_EVENTS,
} TatrLog_Event;

const char   *tatrlog_event_name(TatrLog_Event e);
TatrLog_Event tatrlog_event_from_str(const char *s);

// Parsed entry
//
// On-disk format (blank line terminates each entry):
//
//   event: edit
//   time: 2026-04-20T14:32:01
//   id: 20260418-143201-a3f2c1
//   author: rinelu
//   title: Fix crash on startup
//   ...fields
//
// All key-value pairs are stored in `fields`. `event`, `time`, and `id`
// are promoted to top-level for fast access. They are also present in
// `fields` so tatrlog_get() works uniformly for any key.

typedef struct {
    String_View key;
    String_View val;
} TatrLog_Field;

typedef struct {
    TatrLog_Event event;
    time_t        time;
    String_View   id;

    struct {
        TatrLog_Field *items;
        size_t count;
        size_t capacity;
    } fields;

    String_View body;
    char *_buf; // owns all string data.
} TatrLog_Entry;

typedef struct {
    TatrLog_Entry *items;
    size_t count;
    size_t capacity;
} TatrLog_Entries;

// Linear scan. Returns empty SV if key not found.
String_View tatrlog_get(const TatrLog_Entry *e, const char *key);

// Returns temp-allocated NUL-terminated string, or "" if not found.
const char *tatrlog_get_cstr(const TatrLog_Entry *e, const char *key);

typedef struct {
    TatrLog_Event  event;
    const char    *id;
    String_Builder sb;
    bool           begun;
} TatrLog_Builder;

void tatrlog_begin(TatrLog_Builder *b, TatrLog_Event event, const char *id);

void tatrlog_body   (TatrLog_Builder *b, const char *text);
void tatrlog_field  (TatrLog_Builder *b, const char *key, const char *val);
void tatrlog_fieldsv(TatrLog_Builder *b, const char *key, String_View val);
bool tatrlog_commit(TatrLog_Builder *b);
void tatrlog_discard(TatrLog_Builder *b);

// TLOG(TATRLOG_EDIT, id, {
//     tatrlog_field(&__log, "author", USERNAME_ENV);
//     tatrlog_field(&__log, "field",  "priority");
//     tatrlog_field(&__log, "old",    "normal");
//     tatrlog_field(&__log, "new",    "high");
// });
#define TLOG(event, id, body)                  \
    do {                                       \
        TatrLog_Builder __log = {0};           \
        tatrlog_begin(&__log, (event), (id));  \
        body                                   \
        tatrlog_commit(&__log);                \
    } while (0)

bool tatrlog_load(TatrLog_Entries *out);
void tatrlog_entry_free(TatrLog_Entry *e);
void tatrlog_entries_free(TatrLog_Entries *entries);

#endif // TATRLOG_H_
