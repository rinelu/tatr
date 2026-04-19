#include "astring.h"
#include "cmd.h"

int cmd_delete(int argc, char **argv)
{
    bool *force = clag_bool("force", 'f', false, "Ignore missing issues and suppress errors");
    bool *interactive = clag_bool("interactive", 'i', false, "Prompt before delete");
    clag_usage("<id>... [option]");

    if (!clag_parse(argc, argv)) {
        clag_print_error(stderr);
        return 1;
    }
    if (!require_repo()) return 1;

    if (clag_rest_argc() < 1) {
        log_error("missing issue ID");
        return 1;
    }

    Temp_Checkpoint tmark = temp_save();
    int result = 0;
    for (int i = 0; i < clag_rest_argc(); i++) {
        const char *id = clag_rest_argv()[i];
        Issue iss;
        if (!issue_load(id, &iss)) {
            if (!*force) {
                log_error("issue '%s' not found", id);
            }
            result = 1;
            continue;
        }

        if (*interactive) {
            if (!log_confirm("Delete issue %s ("SV_Fmt")?", id, SV_Arg(iss.title))) {
                log_msg("Skipped %s", id);
                issue_free(&iss);
                continue;
            }
        }

        if (!fs_delete_recursive(iss.dpath)) {
            if (!*force) {
                log_error("failed to delete issue '%s'", id);
                result = 1;
            }
            issue_free(&iss);
            continue;
        }

        tatrlog_append(TATRLOG_DELETE, id, temp_sv_to_cstr(iss.title));
        log_info("Deleted issue %s", id);
        issue_free(&iss);
    }

    temp_rewind(tmark);
    return result;
}
