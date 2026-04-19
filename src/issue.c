#include "issue.h"
#include "astring.h"

#include "fs.h"
#include "log.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void issue__split_header_body(String_View content, String_View *header, String_View *body)
{
    String_View cursor = content;

    while (cursor.count > 0) {
        String_View line = sv_slice_by_delim(&cursor, '\n');

        if (sv_eq_cstr(line, "---")) {
            *header = sv_from_parts(content.data, (size_t)(line.data - content.data - 1));
            *body   = cursor;
            return;
        }
    }

    *header = content;
    *body   = (String_View){0};
}

void issue__combine_raw(String_View header, String_View body, String_Builder *out)
{
    String_Builder tmp = {0};
    sb_append_sv(&tmp, header);

    if (header.count > 0 && header.data[header.count - 1] != '\n') {
        sb_append_char(&tmp, '\n');
    }

    sb_append_cstr(&tmp, "---\n");
    sb_append_sv(&tmp, body);

    sb_free(*out);
    *out = tmp;
}

static void issue__update(Issue *iss)
{
    iss->raw = sb_to_sv(iss->raw_sb);

    issue__split_header_body(iss->raw, &iss->header, &iss->body);

    issue_get_field(iss, "title",    &iss->title);
    issue_get_field(iss, "tags",     &iss->tags);
    issue_get_field(iss, "created",  &iss->created);

    String_View tmp;
    if (issue_get_field(iss, "status", &tmp)) {
        Issue_StatusKind s = issue_status_from_sv(tmp);
        if (s == ISSUE_SINVALID)
             log_warn("invalid status in issue "SV_Fmt, SV_Arg(iss->id));
        else iss->status = s;
    }
    if (issue_get_field(iss, "priority", &tmp)) {
        Issue_PriorityKind p = issue_priority_from_sv(tmp);
        if (p == ISSUE_PINVALID)
             log_warn("invalid priority in issue "SV_Fmt, SV_Arg(iss->id));
        else iss->priority = p;
    }
}

static bool issue__id_is_safe(const char *id)
{
    if (!id || !*id)           return false;
    if (strcmp(id, ".")  == 0) return false;
    if (strcmp(id, "..") == 0) return false;

    for (const char *p = id; *p; p++) {
        if (*p == '/' || *p == '\\') return false;
        // Reject all control characters
        if ((unsigned char)*p < 0x20) return false;
    }
    return true;
}

Issue_StatusKind issue_status_from_cstr(const char *str)
{
    if (!str || !*str) return ISSUE_SINVALID;
    
    if (strcmp(str, "open")        == 0) return ISSUE_SOPEN;
    if (strcmp(str, "closed")      == 0) return ISSUE_SCLOSED;
    if (strcmp(str, "wontfix")     == 0) return ISSUE_SWONTFIX;
    if (strcmp(str, "in-progress") == 0) return ISSUE_SONGOING;

    return ISSUE_SINVALID;
}

Issue_StatusKind issue_status_from_sv(String_View sv)
{
    if (sv_eq_cstr(sv, "open"))    return ISSUE_SOPEN;
    if (sv_eq_cstr(sv, "closed"))  return ISSUE_SCLOSED;
    if (sv_eq_cstr(sv, "wontfix")) return ISSUE_SWONTFIX;
    if (sv_eq_cstr(sv, "in-progress")) return ISSUE_SONGOING;

    return ISSUE_SINVALID;
}

const char *issue_status_to_cstr(Issue_StatusKind s)
{
    switch (s) {
        case ISSUE_SOPEN:    return "open";
        case ISSUE_SCLOSED:  return "closed";
        case ISSUE_SWONTFIX: return "wontfix";
        case ISSUE_SONGOING: return "in-progress";
        default: return "?";
    }
}

Issue_PriorityKind issue_priority_from_cstr(const char *str)
{
    if (!str || !*str) return ISSUE_PINVALID;

    if (strcmp(str, "low")      == 0) return ISSUE_PLOW;
    if (strcmp(str, "normal")   == 0) return ISSUE_PNORMAL;
    if (strcmp(str, "high")     == 0) return ISSUE_PHIGH;
    if (strcmp(str, "critical") == 0) return ISSUE_PCRITICAL;

    return ISSUE_PINVALID;
}

Issue_PriorityKind issue_priority_from_sv(String_View sv)
{
    if (sv_eq_cstr(sv, "low"))      return ISSUE_PLOW;
    if (sv_eq_cstr(sv, "normal"))   return ISSUE_PNORMAL;
    if (sv_eq_cstr(sv, "high"))     return ISSUE_PHIGH;
    if (sv_eq_cstr(sv, "critical")) return ISSUE_PCRITICAL;

    return ISSUE_PNORMAL;
}

const char *issue_priority_to_cstr(Issue_PriorityKind p)
{
    switch (p) {
        case ISSUE_PLOW:      return "low";
        case ISSUE_PNORMAL:   return "normal";
        case ISSUE_PHIGH:     return "high";
        case ISSUE_PCRITICAL: return "critical";
        default: return "?";
    }
}

bool issue_get_field(Issue *iss, const char *key, String_View *out)
{
    String_View cursor = iss->header;

    while (cursor.count > 0) {
        String_View line = sv_slice_by_delim(&cursor, '\n');

        String_View rest = line;
        String_View k = sv_trim(sv_slice_by_delim(&rest, ':'));

        if (!sv_eq(k, sv_from_cstr(key))) continue;

        *out = sv_trim(rest);
        return true;
    }

    return false;
}

bool issue_replace_field(Issue *iss, const char *field, const char *value)
{
    if (!iss->header.data) return false;

    String_View cursor = iss->header;
    String_Builder tmp = {0};
    bool replaced = false;

    while (cursor.count > 0) {
        String_View line = sv_slice_by_delim(&cursor, '\n');

        String_View rest = line;
        String_View k = sv_trim(sv_slice_by_delim(&rest, ':'));

        if (sv_eq(k, sv_from_cstr(field))) {
            sb_appendf(&tmp, "%s: %s\n", field, value);
            replaced = true;
            continue;
        }
        sb_append_sv(&tmp, line);
        sb_append_cstr(&tmp, "\n");
    }

    if (!replaced) {
        sb_free(tmp);
        return false;
    }

    issue__combine_raw(sb_to_sv(tmp), iss->body, &iss->raw_sb);

    sb_free(tmp);

    issue__update(iss);

    return true;
}

bool issue_set_status(Issue *iss, Issue_StatusKind s)
{
    return issue_replace_field(iss, "status", issue_status_to_cstr(s));
}

bool issue_set_priority(Issue *iss, Issue_PriorityKind p)
{
    return issue_replace_field(iss, "priority", issue_priority_to_cstr(p));
}

bool issue_init_empty(Issue *iss)
{
    memset(iss, 0, sizeof(*iss));
    return true;
}

bool issue_load(const char *id, Issue *out)
{
    memset(out, 0, sizeof(*out));

    if (!issue__id_is_safe(id)) {
        log_error("invalid issue ID '%s'", id);
        return false;
    }
    
    const char *dir_path = fs_path(".tatr/issues", id);
    const char *path = fs_path(dir_path, "issue.tatr");
    out->attach_path = fs_path(dir_path, "attachments");
    out->path  = path;
    out->dpath = dir_path;

    if (!fs_read_file(path, &out->raw_sb))
        return false;

    out->id = sv_from_cstr(id);
    issue__update(out);
    return true;
}

void issue_free(Issue *iss)
{
    sb_free(iss->raw_sb);
    memset(iss, 0, sizeof(*iss));
}

bool issue_save(Issue *iss)
{
    bool result = false;
    issue__update(iss);
    String_Builder tmp_path = {0};
    sb_appendf(&tmp_path, "%s.tmp", iss->path);
    sb_append_null(&tmp_path);

    if (!fs_write_file(tmp_path.items, iss->raw.data, iss->raw.count))
        goto defer;

    if (!fs_rename(tmp_path.items, iss->path))
        goto defer;

    result = true;
defer:
    sb_free(tmp_path);
    return result;
}

bool issue_is_closed(const Issue *iss)
{
    return iss->status == ISSUE_SCLOSED ||
           iss->status == ISSUE_SWONTFIX;
}

bool issue_has_tag(const Issue *iss, const char *tag)
{
    return sv_has(iss->tags, tag, ',');
}
