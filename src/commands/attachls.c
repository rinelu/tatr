#include "cmd.h"
#include "tatr.h"

int cmd_attachls(int argc, char **argv)
{
    clag_usage("<id>");

    if (!clag_parse(argc, argv)) {
        clag_print_error(stderr);
        return 1;
    }
    if (!require_repo()) return 1;

    if (clag_rest_argc() < 1) {
        log_error("tatr attachls: missing issue ID");
        return 1;
    }

    const char *id = clag_rest_argv()[0];
    Temp_Checkpoint tmark = temp_save();
    const Renderer *r = renderer_for(TATR_FMT_HUMAN);

    Tatr_Attachment_List_Result result;
    Tatr_Error err = tatr_issue_attachments(id, &result);

    if (err == TATR_ERR_NOT_FOUND) {
        log_error("issue '%s' not found", id);
        temp_rewind(tmark);
        return 1;
    }
    if (err != TATR_OK) {
        log_error("cannot list attachments");
        temp_rewind(tmark);
        return 1;
    }

    r->attachment_list(stdout, &result);

    temp_rewind(tmark);
    return 0;
}
