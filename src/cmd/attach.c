#include "cmd.h"

int cmd_attach(int argc, char **argv)
{
    clag_usage("<id> <file> [<file> ...]");

    if (!clag_parse(argc, argv)) {
        clag_print_error(stderr);
        return 1;
    }

    if (clag_rest_argc() < 2) {
        log_error("Usage: tatr attach <id> <file> [<file> ...]");
        return 1;
    }

    const char *id = clag_rest_argv()[0];

    size_t tmark = temp_save();
    int result = 1;
    Issue iss;
    if (!issue_load(id, &iss)) {
        log_error("issue '%s' not found", id);
        return 1;
    }

    // ensure attachment directory exists
    if (!fs_mkdir(iss.attach_path) && !fs_file_exists(iss.attach_path)) {
        log_error("Cannot create attachment directory");
        goto defer;
    }

    for (int i = 1; i < clag_rest_argc(); i++) {
        const char *src = clag_rest_argv()[i];

        if (!fs_file_exists(src)) {
            log_error("File '%s' not found", src);
            goto defer;
        }

        // extract basename
        const char *base = fs_path_name(src);
        String_Builder dst = {0};
        if (!fs_unique_path(iss.attach_path, base, &dst)) {
            log_error("Failed to create unique path for '%s'", base);
            sb_free(dst);
            goto defer;
        }
        sb_append_null(&dst);
        bool renamed = strcmp(base, strrchr(dst.items, '/') ? strrchr(dst.items, '/') + 1 : dst.items) != 0;

        if (!fs_copy_file(src, dst.items)) {
            log_error("Cannot copy '%s'", src);
            sb_free(dst);
            goto defer;
        }

        if (renamed)
             log_info("Attached %s -> %s (renamed, conflict resolved)", base, dst.items);
        else log_info("Attached %s", base);

        sb_free(dst);
    }
    result = 0;
defer:
    issue_free(&iss);
    temp_rewind(tmark);
    return result;
}
