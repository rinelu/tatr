#include "cmd.h"

int cmd_attachls(int argc, char **argv)
{
    clag_usage("<id>");

    if (!clag_parse(argc, argv)) {
        clag_print_error(stderr);
        return 1;
    }

    if (clag_rest_argc() < 1) {
        log_error("tatr attachls: missing issue ID");
        return 1;
    }

    if (!require_repo()) return 1;

    Temp_Checkpoint tmark = temp_save();
    int result = 1;
    const char *id = clag_rest_argv()[0];
    Issue iss;
    File_Paths files = {0};

    if (!issue_load(id, &iss)) {
        log_error("tatr: issue '%s' not found", id);
        goto defer;
    }

    if (!fs_read_dir(iss.attach_path, &files)) {
        log_error("tatr: cannot list attachments");
        goto defer;
    }

    da_sort(&files, cmp_paths);
    
    result = 0;
    if (files.count == 0) {
        log_msg("(no attachments)");
        goto defer;
    }

    log_msg("Attachments of issue '"SV_Fmt"'", SV_Arg(iss.id));
    for (size_t i = 0; i < files.count; i++)
        log_msg("  - %s", files.items[i]);

defer:
    issue_free(&iss);
    da_free(files);
    temp_rewind(tmark);
    return result;
}
