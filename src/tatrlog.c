#include "tatrlog.h"
#include "fs.h"
#include "util.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static const char *EVENT_NAMES[COUNT_TATRLOG_EVENTS] = {
    [TATRLOG_CREATE]  = "create",
    [TATRLOG_EDIT]    = "edit",
    [TATRLOG_CLOSE]   = "close",
    [TATRLOG_REOPEN]  = "reopen",
    [TATRLOG_DELETE]  = "delete",
    [TATRLOG_TAG]     = "tag",
    [TATRLOG_COMMENT] = "comment",
    [TATRLOG_ATTACH]  = "attach",
    [TATRLOG_DETACH]  = "detach",
};

const char *tatrlog_event_name(TatrLog_Event e)
{
    if (e < 0 || e >= COUNT_TATRLOG_EVENTS) return "unknown";
    return EVENT_NAMES[e];
}

static TatrLog_Event event_from_sv(String_View sv)
{
    for (int i = 0; i < COUNT_TATRLOG_EVENTS; i++)
        if (sv_eq_cstr(sv, EVENT_NAMES[i]))
            return (TatrLog_Event)i;
    return (TatrLog_Event)-1;
}

bool tatrlog_append(TatrLog_Event event, const char *id, const char *detail)
{
    assert(event >= 0 && event < COUNT_TATRLOG_EVENTS);

    char ts[32];
    timestamp_iso(ts, sizeof(ts));

    // Format: "<ts> <event> <id> <detail>\n"
    String_Builder line = {0};
    sb_appendf(&line, "%s %s %s %s\n",
               ts,
               EVENT_NAMES[event],
               id     ? id     : "-",
               detail ? detail : "");
    sb_append_null(&line);

    bool ok = fs_append_file(TATRLOG_PATH, line.items);
    sb_free(line);
    return ok;
}

bool tatrlog_parse_line(String_View line, TatrLog_Entry *out)
{
    // Format: "<ts> <event> <id> <detail...>"
    line = sv_trim(line);
    if (sv_empty(line) || line.data[0] == '#') return false;

    String_View rest = line;
    out->raw_line = line;

    String_View ts_sv  = sv_slice_by_delim(&rest, ' ');
    String_View ev_sv  = sv_slice_by_delim(&rest, ' ');
    String_View id_sv  = sv_slice_by_delim(&rest, ' ');

    if (sv_empty(ts_sv) || sv_empty(ev_sv) || sv_empty(id_sv))
        return false;

    out->time   = parse_time_n(ts_sv.data, ts_sv.count);
    out->event  = event_from_sv(ev_sv);
    out->id     = id_sv;
    out->detail = rest;

    if ((int)out->event < 0) return false;

    return true;
}

bool tatrlog_load(TatrLog_Entries *out)
{
    memset(out, 0, sizeof(*out));

    String_Builder raw = {0};
    if (!fs_read_file(TATRLOG_PATH, &raw)) {
        sb_free(raw);
        return false;
    }
    sb_append_null(&raw);

    String_View cursor = sv_from_parts(raw.items, raw.count - 1);

    while (!sv_empty(cursor)) {
        String_View line = sv_slice_by_delim(&cursor, '\n');
        TatrLog_Entry entry;
        if (!tatrlog_parse_line(line, &entry)) continue;

#define ARGS(x) strndup(entry.x.data, entry.x.count), entry.x.count
        entry.id       = sv_from_parts(ARGS(id));
        entry.detail   = sv_from_parts(ARGS(detail));
        entry.raw_line = sv_from_parts(ARGS(raw_line));
#undef ARGS

        da_append(out, entry);
    }

    sb_free(raw);
    return true;
}

void tatrlog_entries_free(TatrLog_Entries *entries)
{
    for (size_t i = 0; i < entries->count; i++) {
        free((void *)entries->items[i].id.data);
        free((void *)entries->items[i].detail.data);
        free((void *)entries->items[i].raw_line.data);
    }
    da_free(*entries);
}
