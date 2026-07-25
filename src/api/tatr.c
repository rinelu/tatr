#include "tatr.h"

#include "global.h"
#include "fs.h"
#include "temp.h"
#include "util.h"
#include "ui.h"
#include "tatrlog.h"

#include <string.h>
#include <time.h>
#include <stdio.h>

#define TV(sv) sv_from_parts(temp_strndup((sv).data, (sv).count), (sv).count)

const char *tatr_version(void)
{
    return TATR_VERSION;
}

Tatr_Error tatr_issue_show(const char *id, bool raw, Tatr_Issue_View *out)
{
    memset(out, 0, sizeof(*out));

    Issue iss;
    if (!issue_load(id, &iss))
        return TATR_ERR_NOT_FOUND;

    out->id       = TV(iss.id);
    out->title    = TV(iss.title);
    out->created  = TV(iss.created);
    out->tags     = TV(iss.tags);
    out->status   = iss.status;
    out->priority = iss.priority;
    out->raw_mode = raw;

    if (raw) {
        out->raw = TV(iss.raw);
        issue_free(&iss);
        return TATR_OK;
    }

    out->body = TV(iss.body);

    File_Paths files = {0};
    if (fs_read_dir(iss.attach_path, &files)) {
        const char **names = NULL;
        if (files.count > 0) {
            names = temp_alloc(files.count * sizeof(*names));
            for (size_t i = 0; i < files.count; i++)
                names[i] = temp_strdup(files.items[i]);
        }
        out->has_attachments_dir = true;
        out->attachments         = names;
        out->attachment_count    = files.count;
        da_free(files);
    }

    issue_free(&iss);
    return TATR_OK;
}

static bool filter_match(const Issue *iss, const Tatr_Issue_Filter *f)
{
    if (!f->include_closed && issue_is_closed(iss))
        return false;

    if (f->status != ISSUE_SINVALID && iss->status != f->status)
        return false;

    if (f->priority != ISSUE_PINVALID && iss->priority != f->priority)
        return false;

    if (f->tag && !issue_has_tag(iss, f->tag))
        return false;

    return true;
}

Tatr_Error tatr_issue_list(const Tatr_Issue_Filter *filter, bool show_header, Tatr_Issue_List_Result *out)
{
    memset(out, 0, sizeof(*out));
    out->show_header = show_header;

    File_Paths ids = {0};
    if (!issue_list_ids(&ids)) {
        da_free(ids);
        return TATR_ERR_STORAGE;
    }

    da_sort(&ids, cmp_paths);

    Tatr_Issue_Summary_Array matches = {0};

    uint64_t shown = 0;
    for (size_t i = 0; i < ids.count; i++) {
        Issue iss;
        if (!issue_load(ids.items[i], &iss)) continue;

        if (!filter_match(&iss, filter)) {
            issue_free(&iss);
            continue;
        }

        Tatr_Issue_Summary summary = {
            .id       = TV(iss.id),
            .title    = TV(iss.title),
            .status   = iss.status,
            .priority = iss.priority,
        };
        da_append(&matches, summary);
        issue_free(&iss);

        shown++;
        if (filter->limit > 0 && shown >= filter->limit) break;
    }

    if (matches.count) {
        out->issues.items = temp_alloc(matches.count * sizeof(*out->issues.items));
        memcpy(out->issues.items, matches.items, matches.count * sizeof(*out->issues.items));
    }
    out->issues.count = matches.count;
    da_free(matches);

    da_free(ids);
    return TATR_OK;
}

Tatr_Error tatr_issue_attachments(const char *id, Tatr_Attachment_List_Result *out)
{
    memset(out, 0, sizeof(*out));

    Issue iss;
    if (!issue_load(id, &iss))
        return TATR_ERR_NOT_FOUND;

    out->issue_id = TV(iss.id);

    File_Paths files = {0};
    if (!fs_read_dir(iss.attach_path, &files)) {
        issue_free(&iss);
        return TATR_ERR_STORAGE;
    }

    da_sort(&files, cmp_paths);

    if (files.count > 0) {
        const char **names = temp_alloc(files.count * sizeof(*names));
        for (size_t i = 0; i < files.count; i++)
            names[i] = temp_strdup(files.items[i]);
        out->items = names;
        out->count = files.count;
    }

    da_free(files);
    issue_free(&iss);
    return TATR_OK;
}

static bool search_match_token(String_View haystack, String_View token, bool case_sensitive)
{
    return case_sensitive ? sv_contains(haystack, token) : sv_icontains(haystack, token);
}

Tatr_Error tatr_issue_search(const Tatr_Search_Query *q, Tatr_Search_Result *out)
{
    memset(out, 0, sizeof(*out));
    out->query = q->token_count > 0 ? q->tokens[0] : "";

    File_Paths issues = {0};
    if (!issue_list_ids(&issues)) {
        da_free(issues);
        return TATR_ERR_STORAGE;
    }

    da_sort(&issues, cmp_paths);

    Tatr_Issue_Summary_Array matches = {0};

    uint64_t found = 0;
    for (size_t i = 0; i < issues.count; i++) {
        Issue iss = {0};
        if (!issue_load(issues.items[i], &iss)) continue;

        bool match = true;

        for (size_t t = 0; t < q->token_count; t++) {
            String_View tok = sv_from_cstr(q->tokens[t]);

            String_View rest = tok;
            String_View key  = sv_slice_by_delim(&rest, ':');

            if (rest.count > 0) {
                String_View value = rest;

                if (sv_eq_cstr(key, "tag") || sv_eq_cstr(key, "tags")) {
                    if (!sv_has(iss.tags, value.data, ',')) { match = false; break; }
                } else if (sv_eq_cstr(key, "status")) {
                    if (!search_match_token(issue_status_to_sv(iss.status), value, q->case_sensitive)) { match = false; break; }
                } else if (sv_eq_cstr(key, "priority")) {
                    if (!search_match_token(issue_priority_to_sv(iss.priority), value, q->case_sensitive)) { match = false; break; }
                } else if (sv_eq_cstr(key, "title")) {
                    if (!search_match_token(iss.title, value, q->case_sensitive)) { match = false; break; }
                } else {
                    String_View hay = q->header_only ? iss.header : iss.raw;
                    if (!search_match_token(hay, tok, q->case_sensitive)) { match = false; break; }
                }
                continue;
            }

            String_View hay = q->header_only ? iss.header : iss.raw;
            if (!search_match_token(hay, tok, q->case_sensitive)) { match = false; break; }
        }

        if (!match) {
            issue_free(&iss);
            continue;
        }

        Tatr_Issue_Summary summary = {
            .id       = TV(iss.id),
            .title    = TV(iss.title),
            .status   = iss.status,
            .priority = iss.priority,
        };
        da_append(&matches, summary);
        issue_free(&iss);

        found++;
        if (q->limit > 0 && found >= q->limit) break;
    }

    if (matches.count) {
        out->matches.items = temp_alloc(matches.count * sizeof(*out->matches.items));
        memcpy(out->matches.items, matches.items, matches.count * sizeof(*out->matches.items));
    }
    out->matches.count = matches.count;
    da_free(matches);

    da_free(issues);
    return TATR_OK;
}

static bool log_matches_event(const TatrLog_Entry *e, const char *f)
{
    if (!f || !*f) return true;
    return e->event == tatrlog_event_from_str(f);
}

static bool log_matches_id(const TatrLog_Entry *e, const char *id)
{
    if (!id || !*id) return true;
    return strncmp(id, e->id.data, strlen(id)) == 0;
}

static bool log_matches_since(const TatrLog_Entry *e, time_t t)
{
    return t == 0 || e->time >= t;
}

static bool log_matches_until(const TatrLog_Entry *e, time_t t)
{
    return t == 0 || e->time <= t;
}

static bool log_matches_author(const TatrLog_Entry *e, const char *a)
{
    if (!a || !*a) return true;
    String_View sv = tatrlog_get(e, "author");
    return sv.data && sv_icontains(sv, sv_from_cstr(a));
}

Tatr_Error tatr_log_query(const Tatr_Log_Query *q, TatrLog_Entries *entries, Tatr_Log_Result *out)
{
    memset(out, 0, sizeof(*out));
    out->oneline = q->oneline;

    if (!tatrlog_load(entries))
        return TATR_ERR_STORAGE;

    if (entries->count == 0)
        return TATR_OK;

    const TatrLog_Entry **items = temp_alloc(entries->count * sizeof(*items));
    size_t shown = 0;

    long total = (long)entries->count;
    long start = q->reverse ? 0     : total - 1;
    long end   = q->reverse ? total : -1;
    long step  = q->reverse ? 1     : -1;

    for (long i = start; i != end; i += step) {
        const TatrLog_Entry *e = &entries->items[i];

        if (!log_matches_event (e, q->event))     continue;
        if (!log_matches_id    (e, q->id_prefix))  continue;
        if (!log_matches_since (e, q->since))      continue;
        if (!log_matches_until (e, q->until))      continue;
        if (!log_matches_author(e, q->author))     continue;

        items[shown++] = e;
        if (q->limit > 0 && shown >= q->limit) break;
    }

    out->items = items;
    out->count = shown;
    return TATR_OK;
}

// extract latest comment date if exists
static String_View status__last_updated(const Issue *iss)
{
    String_View body = iss->body;
    String_View last = iss->created;

    while (body.count > 0) {
        String_View line = sv_trim(sv_slice_by_delim(&body, '\n'));

        if (sv_starts_with(line, sv_from_cstr("date:"))) {
            String_View rest = line;
            sv_slice_by_delim(&rest, ':');
            last = sv_trim(rest);
        }
    }

    return last;
}

#define status__parse_iso(x) parse_time_n((x).data, (x).count)

typedef struct {
    const char *id;
    String_View title;
    Issue_Status_Kind status;
    Issue_Priority_Kind priority;
    String_View tags;
    String_View created;
    String_View updated;
} Status__Issue_Info;

typedef struct {
    Status__Issue_Info *items;
    size_t count, capacity;
} Status__Issues;

static bool status__is_closed(const Status__Issue_Info *iss)
{
    return iss->status == ISSUE_SCLOSED || iss->status == ISSUE_SWONTFIX;
}

static int status__cmp_recent(const void *a, const void *b)
{
    const Status__Issue_Info *ia = a;
    const Status__Issue_Info *ib = b;

    time_t ta = status__parse_iso(ia->updated);
    time_t tb = status__parse_iso(ib->updated);

    return (tb > ta) - (tb < ta);
}

typedef struct {
    String_View tag;
    size_t count;
} Status__Tag_Count;

typedef struct {
    Status__Tag_Count *items;
    size_t count, capacity;
} Status__Tags;

static int status__tag_find(Status__Tags *tags, String_View t)
{
    for (size_t i = 0; i < tags->count; i++)
        if (sv_eq(tags->items[i].tag, t))
            return (int)i;
    return -1;
}

static int status__cmp_tag_count(const void *a, const void *b)
{
    const Status__Tag_Count *ta = a;
    const Status__Tag_Count *tb = b;
    return (tb->count > ta->count) - (tb->count < ta->count);
}

static Tatr_Status_Issue status__to_result_issue(const Status__Issue_Info *iss)
{
    char rel[32];
    ui_format_time_relative(status__parse_iso(iss->updated), rel, sizeof(rel));

    return (Tatr_Status_Issue){
        .id               = iss->id,
        .title            = iss->title,
        .status           = iss->status,
        .priority         = iss->priority,
        .updated_relative = temp_strdup(rel),
    };
}

Tatr_Error tatr_status(const Tatr_Status_Query *q, Tatr_Status_Result *out)
{
    memset(out, 0, sizeof(*out));
    out->stale_days = q->stale_days;

    File_Paths ids = {0};
    if (!issue_list_ids(&ids)) {
        da_free(ids);
        return TATR_ERR_STORAGE;
    }

    Status__Issues issues = {0};
    Status__Tags   tags   = {0};
    time_t now = time(NULL);

    for (size_t i = 0; i < ids.count; i++) {
        Issue iss;
        if (!issue_load(ids.items[i], &iss)) continue;

        out->total++;
        if (iss.status == ISSUE_SOPEN)          out->open++;
        else if (iss.status == ISSUE_SONGOING)  out->in_progress++;
        else if (issue_is_closed(&iss))         out->closed++;

        Status__Issue_Info info = {
            .id       = temp_strdup(ids.items[i]),
            .title    = TV(iss.title),
            .status   = iss.status,
            .priority = iss.priority,
            .tags     = TV(iss.tags),
            .created  = TV(iss.created),
            .updated  = TV(status__last_updated(&iss)),
        };
        da_append(&issues, info);

        String_View tmp = iss.tags;
        while (tmp.count > 0) {
            String_View t = sv_trim(sv_slice_by_delim(&tmp, ','));
            if (sv_empty(t)) continue;

            int idx = status__tag_find(&tags, t);
            if (idx >= 0) { tags.items[idx].count++; continue; }

            Status__Tag_Count tc = { .tag = TV(t), .count = 1 };
            da_append(&tags, tc);
        }

        issue_free(&iss);
    }

    da_sort(&issues, status__cmp_recent);
    da_sort(&tags,   status__cmp_tag_count);

    typedef struct { Tatr_Status_Issue *items; size_t count, capacity; } Status__Issue_Result_Arr;
    typedef struct { Tatr_Status_Tag_Count *items; size_t count, capacity; } Status__Tag_Result_Arr;

    Status__Issue_Result_Arr high_arr = {0}, stale_arr = {0}, recent_arr = {0};
    Status__Tag_Result_Arr   tags_arr = {0};

    // High priority (open/in-progress only)
    for (size_t i = 0; i < issues.count; i++) {
        Status__Issue_Info *iss = &issues.items[i];
        if (status__is_closed(iss)) continue;
        if (iss->priority != ISSUE_PCRITICAL && iss->priority != ISSUE_PHIGH) continue;
        da_append(&high_arr, status__to_result_issue(iss));
    }

    // Stale
    for (size_t i = 0; i < issues.count; i++) {
        Status__Issue_Info *iss = &issues.items[i];
        if (status__is_closed(iss)) continue;
        double days = difftime(now, status__parse_iso(iss->updated)) / 86400.0;
        if (days < (double)q->stale_days) continue;
        da_append(&stale_arr, status__to_result_issue(iss));
    }

    // Recent activity (already sorted by recency, take first N)
    for (size_t i = 0; i < issues.count && i < q->recent_limit; i++)
        da_append(&recent_arr, status__to_result_issue(&issues.items[i]));

    // Top 5 tags (already sorted by count)
    for (size_t i = 0; i < tags.count && i < 5; i++) {
        Tatr_Status_Tag_Count tc = { .tag = tags.items[i].tag, .count = tags.items[i].count };
        da_append(&tags_arr, tc);
    }

    if (high_arr.count) {
        out->high_priority = temp_alloc(high_arr.count * sizeof(*out->high_priority));
        memcpy(out->high_priority, high_arr.items, high_arr.count * sizeof(*out->high_priority));
    }
    out->high_priority_count = high_arr.count;

    if (stale_arr.count) {
        out->stale = temp_alloc(stale_arr.count * sizeof(*out->stale));
        memcpy(out->stale, stale_arr.items, stale_arr.count * sizeof(*out->stale));
    }
    out->stale_count = stale_arr.count;

    if (recent_arr.count) {
        out->recent = temp_alloc(recent_arr.count * sizeof(*out->recent));
        memcpy(out->recent, recent_arr.items, recent_arr.count * sizeof(*out->recent));
    }
    out->recent_count = recent_arr.count;

    if (tags_arr.count) {
        out->top_tags = temp_alloc(tags_arr.count * sizeof(*out->top_tags));
        memcpy(out->top_tags, tags_arr.items, tags_arr.count * sizeof(*out->top_tags));
    }
    out->top_tags_count = tags_arr.count;

    da_free(high_arr);
    da_free(stale_arr);
    da_free(recent_arr);
    da_free(tags_arr);

    da_free(ids);
    da_free(issues);
    da_free(tags);
    return TATR_OK;
}

#undef status__parse_iso

Tatr_Error tatr_config_list(const Config *c, bool show_local, bool show_global, Tatr_Config_List_Result *out)
{
    memset(out, 0, sizeof(*out));

    bool only_one      = show_local || show_global;
    out->show_local    = only_one ? show_local  : true;
    out->show_global   = only_one ? show_global : true;
    out->show_resolved = !only_one;

    if (out->show_local)  out->local  = &c->local;
    if (out->show_global) out->global = &c->global;

    if (out->show_resolved) {
        Tatr_Config_Resolved_Entry *entries = temp_alloc(CONFIG_KEYS_COUNT * sizeof(*entries));
        for (size_t i = 0; i < CONFIG_KEYS_COUNT; i++) {
            const char *key = CONFIG_KEYS[i].key;
            entries[i] = (Tatr_Config_Resolved_Entry){
                .key    = key,
                .val    = config_get_or_default(c, key),
                .source = store_get_source(c, key),
            };
        }
        out->resolved       = entries;
        out->resolved_count = CONFIG_KEYS_COUNT;
    }

    return TATR_OK;
}

void tatr_config_keys(Tatr_Config_Keys_Result *out)
{
    out->keys  = CONFIG_KEYS;
    out->count = CONFIG_KEYS_COUNT;
}

static const char *close_reason_to_status(const char *reason)
{
    if (!reason) return "closed";
    if (strcmp(reason, "wontfix") == 0) return "wontfix";
    return "closed";
}

Tatr_Error tatr_issue_close(const char *id, const char *reason, const char *author)
{
    Issue iss;
    if (!issue_load(id, &iss))
        return TATR_ERR_NOT_FOUND;

    const char *new_status = close_reason_to_status(reason);

    if (!issue_replace_field(&iss, "status", new_status)) {
        issue_free(&iss);
        return TATR_ERR_IO;
    }

    TLOG(TATRLOG_CLOSE, id, {
        tatrlog_field(&__log, "author", author);
        tatrlog_field(&__log, "title",  temp_strndup(iss.title.data, iss.title.count));
        tatrlog_field(&__log, "status", new_status);
    });

    bool ok = issue_save(&iss);
    issue_free(&iss);
    return ok ? TATR_OK : TATR_ERR_IO;
}

Tatr_Error tatr_issue_reopen(const char *id, const char *author)
{
    (void)author;
    Issue iss;
    if (!issue_load(id, &iss))
        return TATR_ERR_NOT_FOUND;

    if (!issue_replace_field(&iss, "status", "open")) {
        issue_free(&iss);
        return TATR_ERR_IO;
    }

    TLOG(TATRLOG_REOPEN, id, );

    bool ok = issue_save(&iss);
    issue_free(&iss);
    return ok ? TATR_OK : TATR_ERR_IO;
}

Tatr_Error tatr_issue_delete_one(const char *id, const char *author, String_View *out_title)
{
    Issue iss;
    if (!issue_load(id, &iss))
        return TATR_ERR_NOT_FOUND;

    if (out_title) *out_title = TV(iss.title);

    if (!issue_delete(id)) {
        issue_free(&iss);
        return TATR_ERR_IO;
    }

    TLOG(TATRLOG_DELETE, id, {
        tatrlog_field(&__log, "author", author);
        tatrlog_fieldsv(&__log, "title", iss.title);
    });

    issue_free(&iss);
    return TATR_OK;
}

Tatr_Error tatr_issue_add_comment(const char *id, const char *text, const char *author)
{
    Issue iss;
    if (!issue_load(id, &iss))
        return TATR_ERR_NOT_FOUND;

    char ts[64];
    timestamp_iso(ts, sizeof(ts));

    sb_append_cstr(&iss.raw_sb, "\n---comment---\n");
    sb_appendf(&iss.raw_sb, "date: %s\n", ts);
    sb_appendf(&iss.raw_sb, "author: %s\n", author);
    sb_append_cstr(&iss.raw_sb, "\n");
    sb_append_cstr(&iss.raw_sb, text);
    sb_append_cstr(&iss.raw_sb, "\n");

    if (!issue_save(&iss)) {
        issue_free(&iss);
        return TATR_ERR_IO;
    }

    TLOG(TATRLOG_COMMENT, id, {
        tatrlog_field(&__log, "author", author);
        tatrlog_body(&__log, temp_sv_to_cstr(iss.raw));
    });

    issue_free(&iss);
    return TATR_OK;
}

Tatr_Error tatr_issue_tag(const char *id, const Tatr_Tag_Request *req, const char *author, const char **out_conflict_tag)
{
    Issue iss;
    if (!issue_load(id, &iss))
        return TATR_ERR_NOT_FOUND;

    typedef struct { String_View *items; size_t count, capacity; } Tags;
    Tags owned = {0};
    sv_split_by_delim(iss.tags, ',', &owned);

    for (size_t ti = 0; ti < req->count; ti++) {
        const char *t = req->tags[ti];

        int found = -1;
        for (size_t k = 0; k < owned.count; k++) {
            if (!sv_eq_cstr(owned.items[k], t)) continue;
            found = (int)k;
            break;
        }

        if (req->remove) {
            if (found < 0) {
                if (out_conflict_tag) *out_conflict_tag = t;
                da_free(owned);
                issue_free(&iss);
                return TATR_ERR_CONFLICT;
            }
            da_remove_unordered(&owned, (size_t)found);
        } else {
            if (found >= 0) {
                if (out_conflict_tag) *out_conflict_tag = t;
                da_free(owned);
                issue_free(&iss);
                return TATR_ERR_CONFLICT;
            }
            da_append(&owned, sv_from_cstr(t));
        }
    }

    String_Builder sb = {0};
    for (size_t i = 0; i < owned.count; i++) {
        if (i > 0) sb_append_cstr(&sb, ",");
        sb_append_sv(&sb, owned.items[i]);
    }
    sb_append_null(&sb);

    issue_replace_field(&iss, "tags", sb.items);
    if (!issue_save(&iss)) {
        sb_free(sb);
        da_free(owned);
        issue_free(&iss);
        return TATR_ERR_IO;
    }

    String_Builder logtags = {0};
    for (size_t ti = 0; ti < req->count; ti++) {
        if (ti > 0) sb_append(&logtags, ',');
        sb_append_cstr(&logtags, req->tags[ti]);
    }

    if (logtags.count > 1)
        TLOG(TATRLOG_TAG, id, {
            tatrlog_field(&__log, "author", author);
            sb_append_null(&logtags);
            tatrlog_field(&__log, req->remove ? "remove" : "add", logtags.items);
        });

    sb_free(logtags);
    sb_free(sb);
    da_free(owned);
    issue_free(&iss);
    return TATR_OK;
}

Tatr_Error tatr_issue_attach(const char *id, const char *src_path, const char *author, Tatr_Attach_Result *out)
{
    (void)author;
    memset(out, 0, sizeof(*out));

    Issue iss;
    if (!issue_load(id, &iss))
        return TATR_ERR_NOT_FOUND;

    if (!fs_mkdir(iss.attach_path) && !fs_file_exists(iss.attach_path)) {
        issue_free(&iss);
        return TATR_ERR_IO;
    }

    const char *base = fs_path_name(src_path);

    if (!fs_file_exists(src_path)) {
        issue_free(&iss);
        return TATR_ERR_NOT_FOUND;
    }

    String_Builder dst = {0};
    if (!fs_unique_path(iss.attach_path, base, &dst)) {
        sb_free(dst);
        issue_free(&iss);
        return TATR_ERR_IO;
    }
    sb_append_null(&dst);

    const char *dst_base = fs_path_name(dst.items);
    bool renamed = strcmp(base, dst_base) != 0;

    if (!fs_copy_file(src_path, dst.items)) {
        sb_free(dst);
        issue_free(&iss);
        return TATR_ERR_IO;
    }

    TLOG(TATRLOG_ATTACH, id, {
        tatrlog_field(&__log, "file", base);
    });

    out->renamed  = renamed;
    out->dst_name = temp_strdup(dst_base);

    sb_free(dst);
    issue_free(&iss);
    return TATR_OK;
}

Tatr_Error tatr_issue_detach(const char *id, const char *filename, const char *author)
{
    (void)author;
    Issue iss;
    if (!issue_load(id, &iss))
        return TATR_ERR_NOT_FOUND;

    const char *path = fs_path(iss.attach_path, filename);

    if (!fs_file_exists(path)) {
        issue_free(&iss);
        return TATR_ERR_NOT_FOUND;
    }

    if (!fs_delete_file(path)) {
        issue_free(&iss);
        return TATR_ERR_IO;
    }

    TLOG(TATRLOG_DETACH, id, {
        tatrlog_field(&__log, "file", filename);
    });

    issue_free(&iss);
    return TATR_OK;
}

static String_View edit__get_field(const Issue *iss, const char *field)
{
    if (strcmp(field, "title")    == 0) return iss->title;
    if (strcmp(field, "status")   == 0) return issue_status_to_sv(iss->status);
    if (strcmp(field, "priority") == 0) return issue_priority_to_sv(iss->priority);
    if (strcmp(field, "tags")     == 0) return iss->tags;
    if (strcmp(field, "body")     == 0) return iss->body;
    return sv_from_parts(NULL, 0);
}

Tatr_Error tatr_issue_edit_field(const char *id, const char *field, const char *value, const char *author, String_View *out_old)
{
    Issue iss;
    if (!issue_load(id, &iss))
        return TATR_ERR_NOT_FOUND;

    String_View old = edit__get_field(&iss, field);
    if (out_old) *out_old = TV(old);

    if (sv_eq(old, sv_from_cstr(value))) {
        issue_free(&iss);
        return TATR_ERR_CONFLICT; // no-op: unchanged
    }

    char *old_copy = sv_dup(old);

    bool ok;
    if (strcmp(field, "body") == 0) {
        String_Builder tmp = {0};
        sb_append_sv(&tmp, iss.header);
        sb_append_cstr(&tmp, "\n---\n");
        sb_append_cstr(&tmp, value);
        sb_append_cstr(&tmp, "\n");
        sb_free(iss.raw_sb);
        iss.raw_sb = tmp;
        ok = true;
    } else {
        ok = issue_replace_field(&iss, field, value);
    }

    if (!ok) {
        free(old_copy);
        issue_free(&iss);
        return TATR_ERR_IO;
    }

    if (!issue_save(&iss)) {
        free(old_copy);
        issue_free(&iss);
        return TATR_ERR_IO;
    }

    TLOG(TATRLOG_EDIT, iss.id.data, {
        tatrlog_field(&__log, "author", author);
        tatrlog_field(&__log, "title",  temp_strndup(iss.title.data, iss.title.count));
        tatrlog_field(&__log, "field",  field);
        tatrlog_field(&__log, "old",    old_copy);
        tatrlog_field(&__log, "new",    value);
    });

    free(old_copy);
    issue_free(&iss);
    return TATR_OK;
}

Tatr_Error tatr_issue_save_edited(Issue *iss, const char *author)
{
    if (!issue_save(iss))
        return TATR_ERR_IO;

    TLOG(TATRLOG_EDIT, iss->id.data, {
        tatrlog_field(&__log, "author", author);
        tatrlog_field(&__log, "title",  temp_strndup(iss->title.data, iss->title.count));
    });

    return TATR_OK;
}

Tatr_Error tatr_issue_new(const Tatr_Issue_New_Params *p, Issue *iss)
{
    if (p->title) iss->title = sv_from_cstr(p->title);
    iss->status   = issue_status_from_cstr(p->status);
    iss->priority = issue_priority_from_cstr(p->priority);
    iss->tags     = sv_from_cstr(p->tags_csv ? p->tags_csv : "");

    char ts[64];
    timestamp_iso(ts, sizeof(ts));
    iss->created = sv_from_cstr(ts);

    String_Builder raw = {0};
    sb_appendf(&raw, "title: %s\n", iss->title.data ? iss->title.data : "");
    sb_appendf(&raw, "status: %s\n", issue_status_to_cstr(iss->status));
    sb_appendf(&raw, "priority: %s\n", issue_priority_to_cstr(iss->priority));
    sb_appendf(&raw, "created: %s\n", ts);
    sb_appendf(&raw, "tags: %s\n", p->tags_csv ? p->tags_csv : "");
    sb_append_cstr(&raw, "---\n");

    if (p->body.count > 0) {
        sb_append_sv(&raw, p->body);
        if (raw.items[raw.count - 1] != '\n')
            sb_append_cstr(&raw, "\n");
    }

    iss->raw_sb = raw;

    if (!issue_save(iss))
        return TATR_ERR_IO;

    TLOG(TATRLOG_CREATE, p->id, {
        tatrlog_field(&__log, "author", p->author);
        tatrlog_field(&__log, "title",  p->title);
    });

    return TATR_OK;
}

Tatr_Error tatr_issue_new_from_editor(Issue *iss, const char *id, const char *author)
{
    if (!issue_save(iss))
        return TATR_ERR_IO;

    TLOG(TATRLOG_CREATE, id, {
        tatrlog_field(&__log, "author", author);
        tatrlog_field(&__log, "title",  NULL);
    });

    return TATR_OK;
}
