#include "cmd.h"
#include "tatr.h"

int cmd_attach(int argc, char **argv)
{
    clag_usage("<id> <file> [<file> ...]");

    if (!clag_parse(argc, argv)) {
        clag_print_error(stderr);
        return 1;
    }

    if (!require_repo()) return 1;

    if (clag_rest_argc() < 2) {
        clag_print_help(stderr);
        return 1;
    }

    const char *id = clag_rest_argv()[0];

    Temp_Checkpoint tmark = temp_save();
    const Renderer *r = renderer_for(TATR_FMT_HUMAN);
    int result = 1;

    if (!issue_exists(id)) {
        log_error("issue '%s' not found", id);
        temp_rewind(tmark);
        return 1;
    }

    for (int i = 1; i < clag_rest_argc(); i++) {
        const char *src = clag_rest_argv()[i];

        Tatr_Attach_Result att;
        Tatr_Error err = tatr_issue_attach(id, src, NULL, &att);

        if (err == TATR_ERR_NOT_FOUND) {
            log_error("File '%s' not found", src);
            goto defer;
        }
        if (err != TATR_OK) {
            log_error("Cannot copy '%s'", src);
            goto defer;
        }

        if (att.renamed)
            r->message(stdout, temp_sprintf("Attached %s -> %s (renamed, conflict resolved)",
                fs_path_name(src), att.dst_name));
        else
            r->message(stdout, temp_sprintf("Attached %s", fs_path_name(src)));
    }
    result = 0;

defer:
    temp_rewind(tmark);
    return result;
}
