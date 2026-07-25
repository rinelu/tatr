#include "cmd.h"
#include "tatr.h"

int cmd_show(int argc, char **argv)
{
    bool *raw_flag = clag_bool("raw", 'r', false, "Print raw issue.tatr without decoration");
    clag_usage("<id> [options]");

    if (!clag_parse(argc, argv)) {
        clag_print_error(stderr);
        return 1;
    }
    if (!require_repo()) return 1;

    if (clag_rest_argc() < 1) {
        log_error("tatr show: missing issue ID");
        log_msg("usage: tatr show <id> [--raw]");
        return 1;
    }

    const char *id = clag_rest_argv()[0];

    Temp_Checkpoint tmark = temp_save();

    Tatr_Issue_View view;
    Tatr_Error err = tatr_issue_show(id, *raw_flag, &view);

    const Renderer *r = renderer_for(TATR_FMT_HUMAN);
    if (err != TATR_OK) {
        log_error("issue '%s' not found", id);
        temp_rewind(tmark);
        return 1;
    }

    r->issue_view(stdout, &view);

    temp_rewind(tmark);
    return 0;
}
