#include "cmd.h"
#include "tatr.h"

int cmd_comment(int argc, char **argv)
{
    char **message = clag_str("message", 'm', NULL, "Comment text");

    clag_usage("<id> --message <text> [--author <name>]");
    clag_required("message");

    if (!clag_parse(argc, argv)) {
        clag_print_error(stderr);
        return 1;
    }
    if (!require_repo()) return 1;

    if (clag_rest_argc() < 1) {
        log_error("Missing issue ID");
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

    Tatr_Error err = tatr_issue_add_comment(id, *message, author);
    if (err == TATR_ERR_NOT_FOUND) {
        log_error("Issue '%s' not found", id);
        temp_rewind(tmark);
        return 1;
    }
    if (err != TATR_OK) {
        log_error("Cannot write comment to issue %s", id);
        temp_rewind(tmark);
        return 1;
    }

    r->message(stdout, temp_sprintf("Comment added to issue %s", id));

    temp_rewind(tmark);
    return 0;
}
