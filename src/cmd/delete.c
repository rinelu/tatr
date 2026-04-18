#include "astring.h"
#include "cmd.h"

int cmd_delete(int argc, char **argv)
{
    bool *yes = clag_bool("yes", 'y', false, "Skip confirmation prompt");
    clag_usage("<id> [--yes]");

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
    int result = 1;
    Issue iss;
    if (!issue_load(id, &iss)) {
        log_error("issue '%s' not found", id);
        goto defer;
    }

    if (!*yes && !log_confirm("Delete issue %s ("SV_Fmt")?", id, SV_Arg(iss.title))) {
        log_msg("Aborted.");
        result = 0;
        goto defer;
    }

    tatrlog_append(TATRLOG_DELETE, id, temp_sv_to_cstr(iss.title));
    if (!fs_delete_recursive(iss.dpath)) {
        log_error("failed to delete issue '%s'", id);
        goto defer;
    }

    log_info("Deleted issue %s", id);
    result = 0;
defer:
    issue_free(&iss);
    temp_rewind(tmark);
    return result;
}
