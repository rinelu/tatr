#include "cmd.h"
#include "editor.h"
#include "issue.h"
#include "temp.h"

static String_View get_field(const Issue *iss, const char *field)
{
    if (strcmp(field, "title")    == 0) return iss->title;
    if (strcmp(field, "status")   == 0)
        return sv_from_cstr(issue_status_to_cstr(iss->status));
    if (strcmp(field, "priority") == 0)
        return sv_from_cstr(issue_priority_to_cstr(iss->priority));
    if (strcmp(field, "tags")     == 0) return iss->tags;
    if (strcmp(field, "body")     == 0) return iss->body;
    return sv_from_parts(NULL, 0);
}

static int edit_full(Issue *iss)
{
    String_Builder edited = {0};
    if (!editor_edit(iss->raw.data, iss->raw.count, ".tatr", &edited)) {
        sb_free(edited);
        return 1;
    }

    // Abort if unchanged
    String_View ev = sb_to_sv(edited);
    if (sv_eq(ev, iss->raw)) {
        log_warn("no changes made");
        sb_free(edited);
        return 0;
    }

    sb_free(iss->raw_sb);
    iss->raw_sb = edited;
    issue_save(iss);
    tatrlog_append(TATRLOG_EDIT, temp_sv_to_cstr(iss->id), "full-edit");
    log_info("issue %.*s updated", (int)iss->id.count, iss->id.data);
    return 0;
}

static int edit_field_in_editor(Issue *iss, const char *field)
{
    String_View current = get_field(iss, field);
    const char *suffix  = strcmp(field, "body") == 0 ? ".md" : ".txt";
    String_Builder edited = {0};

    if (!editor_edit(current.data ? current.data : "",
                        current.data ? current.count : 0,
                        suffix, &edited)) {
        sb_free(edited);
        return 1;
    }

    // Strip trailing newline added by editors for header fields
    String_View ev = sb_to_sv(edited);
    if (strcmp(field, "body") != 0) ev = sv_trim_right(ev);

    if (sv_eq(ev, current)) {
        log_warn("no changes made");
        sb_free(edited);
        return 0;
    }

    if (strcmp(field, "body") == 0) {
        String_Builder tmp = {0};
        sb_append_sv(&tmp, iss->header);
        sb_append_cstr(&tmp, "\n---\n");
        sb_append_sv(&tmp, ev);
        if (ev.count > 0 && ev.data[ev.count - 1] != '\n')
            sb_append_cstr(&tmp, "\n");

        sb_free(iss->raw_sb);
        iss->raw_sb = tmp;
    } else {
        char *val = sv_dup(ev);
        bool ok = issue_replace_field(iss, field, val);
        free(val);
        if (!ok) {
            log_error("failed to update '%s'", field);
            sb_free(edited);
            return 1;
        }
    }

    issue_save(iss);
    tatrlog_append(TATRLOG_EDIT, temp_sv_to_cstr(iss->id), temp_sprintf("field=%s", field));
    log_info("updated %s in issue "SV_Fmt, field, SV_Arg(iss->id));
    sb_free(edited);
    return 0;
}

static int edit_field_direct(Issue *iss, const char *field, const char *value)
{
    String_View old = get_field(iss, field);

    if (sv_eq(old, sv_from_cstr(value))) {
        log_warn("no change: '%s' is already '%s'", field, value);
        return 0;
    }

    char *old_copy = sv_dup(old);

    bool ok;
    if (strcmp(field, "body") == 0) {
        String_Builder tmp = {0};
        sb_append_sv(&tmp, iss->header);
        sb_append_cstr(&tmp, "\n---\n");
        sb_append_cstr(&tmp, value);
        sb_append_cstr(&tmp, "\n");
        sb_free(iss->raw_sb);
        iss->raw_sb = tmp;
        ok = true;
    } else {
        ok = issue_replace_field(iss, field, value);
    }

    if (!ok) {
        free(old_copy);
        log_error("failed to update '%s'", field);
        return 1;
    }

    log_info("updated %s in issue "SV_Fmt, field, SV_Arg(iss->id));
    log_msg("  old: %s", old_copy);
    log_msg("  new: %s", value);

    issue_save(iss);
    tatrlog_append(TATRLOG_EDIT, temp_sv_to_cstr(iss->id),
        temp_sprintf("field=%s old=%s new=%s", field, old_copy, value));
    free(old_copy);
    return 0;
}

int cmd_edit(int argc, char **argv)
{
    char **field = clag_str("field", 'f', NULL, "Field:");
    char **value = clag_str("value", 'v', NULL, "New value (omit to open $EDITOR)");

    clag_usage("<id> [--field <field> [--value <value>]]");
    clag_choices("field", "title", "status", "priority", "tags", "body");
    clag_depends("value", "field");

    if (!clag_parse(argc, argv)) {
        clag_print_error(stderr);
        return 1;
    }
    if (!require_repo()) return 1;

    if (clag_rest_argc() < 1) {
        log_error("missing issue ID");
        return 1;
    }

    const char *id = clag_rest_argv()[0];
    Temp_Checkpoint tmark = temp_save();
    int    result  = 1;
    Issue  iss;

    if (!issue_load(id, &iss)) {
        log_error("issue '%s' not found", id);
        goto defer;
    }

    if (!*field) {
        result = edit_full(&iss);
    } else if (!*value) {
        result = edit_field_in_editor(&iss, *field);
    } else {
        result = edit_field_direct(&iss, *field, *value);
    }

defer:
    issue_free(&iss);
    temp_rewind(tmark);
    return result;
}
