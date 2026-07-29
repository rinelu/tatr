#include "cmd.h"
#include "tatr.h"

int cmd_close(int argc, char **argv)
{
    char **reason = clag_str("reason", 'r', NULL, "Optional close reason: fixed | wontfix | duplicate");
    clag_usage("<id> [options]");

    if (!clag_parse(argc, argv)) {
        clag_print_error(stderr);
        return 1;
    }
    if (!require_repo()) return 1;

    if (clag_rest_argc() < 1) {
        log_error("tatr close: missing issue ID");
        return 1;
    }

    const char *id = clag_rest_argv()[0];

    Config cfg = {0};
    config_load(&cfg);
    const char *author = config_get(&cfg, "author");
    if (!author) author = USERNAME_ENV;
    config_free(&cfg);

    Temp_Checkpoint tmark = temp_save();
    const Renderer *r = renderer_for(TATR_FMT_HUMAN);

    const char *new_status = (*reason && strcmp(*reason, "wontfix") == 0) ? "wontfix" : "closed";

    Tatr_Error err = tatr_issue_close(id, *reason, author);
    if (err == TATR_ERR_NOT_FOUND) {
        log_error("issue '%s' not found", id);
        temp_rewind(tmark);
        return 1;
    }
    if (err != TATR_OK) {
        log_error("failed to close issue");
        temp_rewind(tmark);
        return 1;
    }

    r->message(stdout, temp_sprintf("Closed issue %s  (status: %s)", id, new_status));

    temp_rewind(tmark);
    return 0;
}
