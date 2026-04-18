#include "cmd.h"

int cmd_show(int argc, char **argv)
{
    bool *raw_flag = clag_bool("raw", 'r', false, "Print raw issue.tatr without decoration");
    clag_usage("<id> [options]");

    if (!clag_parse(argc, argv)) {
        clag_print_error(stderr);
        return 1;
    }

    if (clag_rest_argc() < 1) {
        log_error("tatr show: missing issue ID");
        log_msg("usage: tatr show <id> [--raw]");
        return 1;
    }

    const char *id = clag_rest_argv()[0];

    int result = 1;
    Temp_Checkpoint tmark = temp_save();
    Issue iss;
    if (!issue_load(id, &iss)) {
        log_error("tatr: issue '%s' not found", id);
        goto defer;
    }

    result = 0;
    if (*raw_flag) {
        log_msg(SV_Fmt, SV_Arg(iss.raw));
        goto defer;
    }

    ui_print_issue_full(&iss);

    File_Paths files = {0};
    if (fs_read_dir(iss.attach_path, &files)) {
        log_msg("\nAttachments (%zu):", files.count);
        for (size_t i = 0; i < files.count; i++)
            log_msg("  %s", files.items[i]);
        da_free(files);
    }

defer:
    issue_free(&iss);
    temp_rewind(tmark);
    return result;
}
