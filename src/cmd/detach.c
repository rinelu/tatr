#include "cmd.h"

int cmd_detach(int argc, char **argv)
{
    bool *yes = clag_bool("yes", 'y', false, "Skip confirmation prompt");
    clag_usage("<id> <filename> [--yes]");

    if (!clag_parse(argc, argv)) {
        clag_print_error(stderr);
        return 1;
    }

    if (clag_rest_argc() < 2) {
        log_error("Usage: tatr detach <id> <filename> [--yes]");
        return 1;
    }

    if (!require_repo()) return 1;

    Temp_Checkpoint tmark = temp_save();
    int result = 1;

    const char *id       = clag_rest_argv()[0];
    const char *filename = clag_rest_argv()[1];

    Issue iss;
    if (!issue_load(id, &iss)) {
        log_error("Issue '%s' not found", id);
        return 1;
    }

    const char *src = fs_path(iss.attach_path, filename);
    if (!fs_file_exists(src)) {
        log_error("Attachment '%s' not found in issue %s", filename, id);
        goto defer;
    }

    if (!*yes) {
        if (!log_confirm("Remove attachment '%s' from issue %s?", filename, id)) {
            log_msg("Aborted.");
            goto defer;
        }
    }

    if (!fs_delete_file(src)) {
        log_error("Failed to remove attachment '%s'", filename);
        goto defer;
    }

    tatrlog_append(TATRLOG_DETACH, id, temp_sprintf("file=%s", filename));

    log_info("Removed '%s' from issue %s", filename, id);

    result = 0;

defer:
    issue_free(&iss);
    temp_rewind(tmark);
    return result;
}
