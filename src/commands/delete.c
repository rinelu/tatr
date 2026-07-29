#include "cmd.h"
#include "tatr.h"

int cmd_delete(int argc, char **argv)
{
    bool *force = clag_bool("force", 'f', false, "Ignore missing issues and suppress errors");
    bool *interactive = clag_bool("interactive", 'i', false, "Prompt before delete");
    clag_usage("<id>... [options]");

    if (!clag_parse(argc, argv)) {
        clag_print_error(stderr);
        return 1;
    }
    if (!require_repo()) return 1;

    if (clag_rest_argc() < 1) {
        log_error("missing issue ID");
        return 1;
    }

    Config cfg = {0};
    config_load(&cfg);
    const char *author = config_get(&cfg, "author");
    if (!author) author = USERNAME_ENV;
    config_free(&cfg);

    Temp_Checkpoint tmark = temp_save();
    const Renderer *r = renderer_for(TATR_FMT_HUMAN);
    int result = 0;

    for (int i = 0; i < clag_rest_argc(); i++) {
        const char *id = clag_rest_argv()[i];

        if (*interactive) {
            Tatr_Issue_View view;
            if (tatr_issue_show(id, false, &view) != TATR_OK) {
                if (!*force) log_error("issue '%s' not found", id);
                result = 1;
                continue;
            }
            if (!log_confirm("Delete issue %s ("SV_Fmt")?", id, SV_Arg(view.title))) {
                log_msg("Skipped %s", id);
                continue;
            }
        }

        String_View title;
        Tatr_Error err = tatr_issue_delete_one(id, author, &title);

        if (err == TATR_ERR_NOT_FOUND) {
            if (!*force) log_error("issue '%s' not found", id);
            result = 1;
            continue;
        }
        if (err != TATR_OK) {
            if (!*force) log_error("failed to delete issue '%s'", id);
            result = 1;
            continue;
        }

        r->message(stdout, temp_sprintf("Deleted issue %s", id));
    }

    temp_rewind(tmark);
    return result;
}
