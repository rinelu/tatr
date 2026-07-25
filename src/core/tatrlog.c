#include "tatrlog.h"
#include "astring.h"
#include "fs.h"
#include "global.h"
#include "temp.h"
#include "util.h"

#include <assert.h>
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
    if ((int)e < 0 || e >= COUNT_TATRLOG_EVENTS) return "unknown";
    return EVENT_NAMES[e];
}

TatrLog_Event tatrlog_event_from_str(const char *s)
{
    if (!s) return (TatrLog_Event)-1;
    for (int i = 0; i < COUNT_TATRLOG_EVENTS; i++)
        if (strcmp(s, EVENT_NAMES[i]) == 0)
            return (TatrLog_Event)i;
    return (TatrLog_Event)-1;
}

String_View tatrlog_get(const TatrLog_Entry *e, const char *key)
{
    da_foreach(TatrLog_Field, f, &e->fields)
        if (sv_eq_cstr(f->key, key))
            return f->val;
    return sv_from_parts(NULL, 0);
}

const char *tatrlog_get_cstr(const TatrLog_Entry *e, const char *key)
{
    String_View sv = tatrlog_get(e, key);
    if (!sv.data || sv.count == 0) return "";
    return temp_strndup(sv.data, sv.count);
}

void tatrlog_begin(TatrLog_Builder *b, TatrLog_Event event, const char *id)
{
    assert(!b->begun && "tatrlog_begin: builder already in use");
    memset(b, 0, sizeof(*b));
    b->event = event;
    b->id    = id;
    b->begun = true;

    char ts[32];
    timestamp_iso(ts, sizeof(ts));

    sb_append_cstr(&b->sb, TATRLOG_SEPARATOR "\n");
    sb_appendf    (&b->sb, "event: %s\n", tatrlog_event_name(event));
    sb_appendf    (&b->sb, "time: %s\n",  ts);
    sb_appendf    (&b->sb, "id: %s\n",    id ? id : "-");
}

void tatrlog_body(TatrLog_Builder *b, const char *text)
{
    assert(b->begun && "tatrlog_body: call tatrlog_begin first");
    if (!text || !*text) return;
    sb_append_char(&b->sb, '\n');
    sb_append_cstr(&b->sb, text);
    if (text[strlen(text) - 1] != '\n')
        sb_append_char(&b->sb, '\n');
}

void tatrlog_field(TatrLog_Builder *b, const char *key, const char *val)
{
    assert(b->begun && "tatrlog_field: call tatrlog_begin first");
    if (!val || !*val) return;
    sb_appendf(&b->sb, "%s: %s\n", key, val);
}

void tatrlog_fieldsv(TatrLog_Builder *b, const char *key, String_View val)
{
    assert(b->begun && "tatrlog_fieldsv: call tatrlog_begin first");
    if (!val.data || val.count == 0) return;
    sb_appendf(&b->sb, "%s: "SV_Fmt"\n", key, SV_Arg(val));
}

bool tatrlog_commit(TatrLog_Builder *b)
{
    assert(b->begun && "tatrlog_commit: nothing to commit");
    sb_append_cstr(&b->sb, "\n");
    sb_append_null(&b->sb);

    bool ok = fs_append_file(TATR_LOG_PATH, b->sb.items);
    tatrlog_discard(b);
    return ok;
}

void tatrlog_discard(TatrLog_Builder *b)
{
    sb_free(b->sb);
    memset(b, 0, sizeof(*b));
}

// Parse "key: value" line.
// Returns false if failed.
static bool parse_kv(String_View line, String_View *key, String_View *val)
{
    String_View rest = line;
    *key = sv_trim(sv_slice_by_delim(&rest, ':'));
    *val = sv_trim(rest);
    return !sv_empty(*key);
}

// Copy a String_View into a flat buffer at *offset. Advances *offset.
// NUL-terminates for safety. Returns SV pointing into buffer.
static String_View buf_copy(char *buf, size_t *off, String_View sv)
{
    if (!sv.data || sv.count == 0)
        return sv_from_parts(buf + *off, 0);
    memcpy(buf + *off, sv.data, sv.count);
    buf[*off + sv.count] = '\0';
    String_View r = sv_from_parts(buf + *off, sv.count);
    *off += sv.count + 1;
    return r;
}

static size_t buf_needed(TatrLog_Field *fs, size_t n)
{
    size_t total = 0;
    for (size_t i = 0; i < n; i++) {
        total += fs[i].key.count + 1;
        total += fs[i].val.count + 1;
    }
    return total;
}

// Flush accumulated tmp_fields into a heap-owned TatrLog_Entry and
// append it to *out. Resets tmp_count to 0.
static void flush_entry(TatrLog_Field *tmp, size_t *tmp_count, String_Builder *body_sb, TatrLog_Entries *out)
{
    if (*tmp_count == 0 && body_sb->count == 0) return;

    size_t bufsz = buf_needed(tmp, *tmp_count) + body_sb->count + 1;
    char  *buf   = (char *)malloc(bufsz > 0 ? bufsz : 1);
    assert(buf && "tatrlog: out of memory");

    TatrLog_Entry e = {0};
    e._buf = buf;
    size_t off = 0;

    for (size_t i = 0; i < *tmp_count; i++) {
        TatrLog_Field f;
        f.key = buf_copy(buf, &off, tmp[i].key);
        f.val = buf_copy(buf, &off, tmp[i].val);
        da_append(&e.fields, f);

        if (sv_eq_cstr(f.key, "event"))
            e.event = tatrlog_event_from_str( temp_strndup(f.val.data, f.val.count));
        else if (sv_eq_cstr(f.key, "time"))
            e.time = parse_time_n(f.val.data, f.val.count);
        else if (sv_eq_cstr(f.key, "id"))
            e.id = f.val;
    }

    if (body_sb->count > 0) {
        memcpy(buf + off, body_sb->items, body_sb->count);
        buf[off + body_sb->count] = '\0';
        e.body = sv_from_parts(buf + off, body_sb->count);
    }

    da_append(out, e);
    *tmp_count = 0;
    body_sb->count = 0;
}

bool tatrlog_load(TatrLog_Entries *out)
{
    memset(out, 0, sizeof(*out));

    String_Builder raw = {0};
    if (!fs_read_file(TATR_LOG_PATH, &raw)) {
        sb_free(raw);
        return false;
    }
    sb_append_null(&raw);

    String_View cursor = sv_from_parts(raw.items, raw.count - 1);
    String_Builder body_sb = {0};

#define MAX_FIELDS 64
    TatrLog_Field tmp[MAX_FIELDS];
    size_t tmp_count = 0;
    bool   in_entry  = false;
    bool   in_body   = false;

    static const String_View SEP = {
        TATRLOG_SEPARATOR,
        sizeof(TATRLOG_SEPARATOR) - 1
    };

    while (!sv_empty(cursor)) {
        String_View line = sv_trim_right(sv_slice_by_delim(&cursor, '\n'));
        if (sv_eq(line, SEP)) {
            flush_entry(tmp, &tmp_count, &body_sb, out);
            in_entry = true;
            in_body  = false;
            continue;
        }

        if (!in_entry) continue;
        if (sv_empty(line)) {
            if (in_body) sb_append_char(&body_sb, '\n');
            in_body = true;
            continue;
        }
        if (in_body) {
            sb_append_sv(&body_sb, line);
            sb_append_char(&body_sb, '\n');
            continue;
        }
        String_View key, val;
        if (parse_kv(line, &key, &val)) {
            if (tmp_count < MAX_FIELDS) {
                tmp[tmp_count].key = key;
                tmp[tmp_count].val = val;
                tmp_count++;
            }
        } else {
            in_body = true;
            sb_append_sv  (&body_sb, line);
            sb_append_char(&body_sb, '\n');
        }
    }

    flush_entry(tmp, &tmp_count, &body_sb, out);
    sb_free(body_sb);

#undef MAX_FIELDS

    sb_free(raw);
    return true;
}

void tatrlog_entry_free(TatrLog_Entry *e)
{
    da_free(e->fields);
    free(e->_buf);
    memset(e, 0, sizeof(*e));
}

void tatrlog_entries_free(TatrLog_Entries *entries)
{
    for (size_t i = 0; i < entries->count; i++)
        tatrlog_entry_free(&entries->items[i]);
    da_free(*entries);
}
