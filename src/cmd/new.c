#include "cmd.h"

int cmd_new(int argc, char **argv)
{
    char    **title    = clag_str ("title",    't', NULL,     "Issue title");
    char    **priority = clag_str ("priority", 'p', "normal", "Priority: low | normal | high | critical");
    char    **status   = clag_str ("status",   's', "open",   "Initial status");
    ClagList *tags     = clag_list("tag",      'T', ',',      "Comma-separated tags or repeat -T");
    char    **body     = clag_str ("body",     'b', NULL,     "Issue body text (optional, inline)");
    char    **file     = clag_str ("file",     'F', NULL,     "Read body from file ('-' for stdin)");

    clag_required("title");
    clag_usage("[options]  (creates a new issue)");

    if (!clag_parse(argc, argv)) {
        clag_print_error(stderr);
        clag_print_options(stderr);
        return 1;
    }

    const char *valid_priorities[] = { "low", "normal", "high", "critical", NULL };
    if (!str_in(*priority, valid_priorities)) {
        ui_error("Invalid priority '%s'. Use: low | normal | high | critical", *priority);
        return 1;
    }

    const char *valid_statuses[] = { "open", "closed", "in-progress", "wontfix", NULL };
    if (!str_in(*status, valid_statuses)) {
        ui_error("Invalid status '%s'. Use: open | closed | in-progress | wontfix", *status);
        return 1;
    }

    if (!require_repo()) return 1;

    size_t tmark = temp_save();
    int result = 1;
    char id[32];
    timestamp_id(id, sizeof(id));

    const char *path = fs_path(".tatr/issues", id);

    if (!fs_mkdir(path)) {
        ui_error("Cannot create issue directory for '%s'", id);
        goto defer;
    }

    path = fs_path(".tatr/issues", id, "attachments");
    if (!fs_mkdir(path)) {
        ui_error("Cannot create attachments directory for '%s'", id);
        goto defer;
    }

    String_Builder body_text = {0};
    if (*file) {
        if (!fs_read_file(*file, &body_text)) {
            ui_error("Cannot read body from '%s'", *file);
            goto defer;
        }
    } else if (*body) {
        sb_append_cstr(&body_text, *body);
    }

    String_Builder tags_sb = {0};
    for (size_t i = 0; i < tags->count; i++) {
        if (i > 0) sb_append(&tags_sb, ',');
        sb_append_cstr(&tags_sb, tags->items[i]);
    }
    sb_append_null(&tags_sb);

    char ts[64];
    timestamp_iso(ts, sizeof(ts));

    String_Builder content = {0};
    sb_appendf(&content, "title: %s\n", *title);
    sb_appendf(&content, "status: %s\n", *status);
    sb_appendf(&content, "priority: %s\n", *priority);
    sb_appendf(&content, "created: %s\n", ts);
    sb_appendf(&content, "tags: %s\n", tags_sb.items);
    sb_append_cstr(&content, "---\n");

    if (body_text.count > 0) {
        sb_append_sv(&content, sb_to_sv(body_text));
        sb_append(&content, '\n');
    }

    sb_append_null(&content);

    path = fs_path(".tatr/issues", id, "issue.tatr");
    bool ok = fs_write_file(path, content.items, content.count - 1);
    if (!ok) {
        ui_error("Cannot write issue file for '%s'", id);
        goto defer;
    }

    ui_info("Created issue %s", id);
    result = 0;
defer:
    sb_free(content);
    sb_free(tags_sb);
    sb_free(body_text);
    temp_rewind(tmark);
    return result;
}
