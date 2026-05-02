#include "cmd.h"

int cmd_detach(int argc, char **argv)
{
    bool *force       = clag_bool("force",       'f', false, "Ignore missing attachments and suppress errors");
    bool *interactive = clag_bool("interactive", 'i', false, "Prompt before removing each attachment");

    clag_usage("<id> <file>... [options]");

    if (!clag_parse(argc, argv)) {
        clag_print_error(stderr);
        return 1;
    }
    if (!require_repo()) return 1;

    if (clag_rest_argc() < 2) {
        log_error("missing issue ID or attachment");
        return 1;
    }

    const char *id = clag_rest_argv()[0];

    Temp_Checkpoint tmark = temp_save();
    int result = 0;

    Issue iss;
    if (!issue_load(id, &iss)) {
        if (!*force) log_error("issue '%s' not found", id);
        temp_rewind(tmark);
        return 1;
    }

    for (int i = 1; i < clag_rest_argc(); i++) {
        const char *filename = clag_rest_argv()[i];

        const char *path = fs_path(iss.attach_path, filename);

        if (!fs_file_exists(path)) {
            if (!*force) {
                log_error("attachment '%s' not found in issue %s", filename, id);
                result = 1;
            }
            continue;
        }

        if (*interactive) {
            if (!log_confirm("Remove '%s' from issue %s?", filename, id)) {
                log_msg("Skipped %s", filename);
                continue;
            }
        }

        if (!fs_delete_file(path)) {
            if (!*force) {
                log_error("failed to remove '%s'", filename);
                result = 1;
            }
            continue;
        }
        TLOG(TATRLOG_DETACH, id, {
            tatrlog_field(&__log, "file", filename);
        });
        log_info("Removed '%s' from issue %s", filename, id);
    }

    issue_free(&iss);
    temp_rewind(tmark);
    return result;
}
