#ifndef TATRLOG_H_
#define TATRLOG_H_

#include "astring.h"
#include <stdbool.h>
#include <stddef.h>
#include <time.h>
#include "global.h"

typedef enum {
    TATRLOG_CREATE  = 0,
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

typedef struct {
    time_t        time;
    TatrLog_Event event;
    String_View   id;
    String_View   detail;   // raw detail string, event-specific
    String_View   raw_line; // full original line
} TatrLog_Entry;

typedef struct {
    TatrLog_Entry *items;
    size_t count;
    size_t capacity;
} TatrLog_Entries;

// Write API  (called by mutating commands)
// Detail is free-form but should follow the convention:
//   edit    ->  "field=<f> old=<v> new=<v>"
//   close   ->  "status=<s>"
//   tag     ->  "add=<t>" or "remove=<t>"
//   attach  ->  "file=<name>"
//   detach  ->  "file=<name>"
//   comment ->  "author=<a>"  (empty string if no author)
//   create  ->  "<title>"
//   delete  ->  "<title>"
//   reopen  ->  ""

const char *tatrlog_event_name(TatrLog_Event e);

bool tatrlog_append(TatrLog_Event event, const char *id, const char *detail);
bool tatrlog_parse_line(String_View line, TatrLog_Entry *out);
bool tatrlog_load(TatrLog_Entries *out);

void tatrlog_entries_free(TatrLog_Entries *entries);

#endif // TATRLOG_H_
