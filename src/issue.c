#include "issue.h"
#include "astring.h"
#include "fs.h"
#include "ui.h"

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
    issue_get_field(iss, "status",   &iss->status);
    issue_get_field(iss, "priority", &iss->priority);
    issue_get_field(iss, "tags",     &iss->tags);
    issue_get_field(iss, "created",  &iss->created);
}

static bool issue__id_is_safe(const char *id)
{
    if (!id || !*id)          return false;
    if (strcmp(id, ".")  == 0) return false;
    if (strcmp(id, "..") == 0) return false;

    for (const char *p = id; *p; p++) {
        if (*p == '/' || *p == '\\') return false;
        // Reject all control characters
        if ((unsigned char)*p < 0x20) return false;
    }
    return true;
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
    return sv_eq(iss->status, sv_from_cstr("closed")) ||
           sv_eq(iss->status, sv_from_cstr("wontfix"));
}

bool issue_has_tag(const Issue *iss, const char *tag)
{
    return sv_has(iss->tags, tag, ',');
}
