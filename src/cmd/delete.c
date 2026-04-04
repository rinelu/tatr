#include "cmd.h"

int cmd_delete(int argc, char **argv)
{
    bool *yes = clag_bool("yes", 'y', false, "Skip confirmation prompt");
    clag_usage("<id> [--yes]");

    if (!clag_parse(argc, argv)) {
        clag_print_error(stderr);
        clag_print_options(stderr);
        return 1;
    }

    if (clag_rest_argc() < 1) {
        ui_error("missing issue ID");
        return 1;
    }

    const char *id = clag_rest_argv()[0];

    size_t tmark = temp_save();
    int result = 1;
    Issue iss;
    if (!issue_load(id, &iss)) {
        ui_error("issue '%s' not found", id);
        goto defer;
    }

    if (!*yes && !ui_confirm("Delete issue %s ("SV_Fmt")?", id, SV_Arg(iss.title))) {
        ui_msg("Aborted.");
        result = 0;
        goto defer;
    }

    if (!fs_delete_recursive(iss.dpath)) {
        ui_error("failed to delete issue '%s'", id);
        goto defer;
    }

    ui_info("Deleted issue %s", id);
    result = 0;
defer:
    issue_free(&iss);
    temp_rewind(tmark);
    return result;
}
