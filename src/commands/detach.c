#include "cmd.h"
#include "tatr.h"

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
    const Renderer *r = renderer_for(TATR_FMT_HUMAN);
    int result = 0;

    if (!issue_exists(id)) {
        if (!*force) log_error("issue '%s' not found", id);
        temp_rewind(tmark);
        return 1;
    }

    for (int i = 1; i < clag_rest_argc(); i++) {
        const char *filename = clag_rest_argv()[i];

        if (*interactive) {
            if (!log_confirm("Remove '%s' from issue %s?", filename, id)) {
                log_msg("Skipped %s", filename);
                continue;
            }
        }

        Tatr_Error err = tatr_issue_detach(id, filename, NULL);

        if (err == TATR_ERR_NOT_FOUND) {
            if (!*force) {
                log_error("attachment '%s' not found in issue %s", filename, id);
                result = 1;
            }
            continue;
        }
        if (err != TATR_OK) {
            if (!*force) {
                log_error("failed to remove '%s'", filename);
                result = 1;
            }
            continue;
        }

        r->message(stdout, temp_sprintf("Removed '%s' from issue %s", filename, id));
    }

    temp_rewind(tmark);
    return result;
}
