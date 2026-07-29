#include "cmd.h"
#include "global.h"
#include "schema.h"

int cmd_init(int argc, char **argv)
{
    bool *force = clag_bool("force", 'f', false, "reinitialize even if .tatr already exists");
    clag_usage("[options]");

    if (!clag_parse(argc, argv)) {
        clag_print_error(stderr);
        return 1;
    }

    if (fs_file_exists(TATR_DIR_PATH) && !*force) {
        log_error(".tatr already exists (use --force or -f to reinitialize)");
        return 1;
    }

    fs_mkdir(TATR_DIR_PATH);
    if (!fs_mkdir_force(TATR_ISSUES_PATH, force)) {
        log_error("cannot create .tatr/issues");
        return 1;
    }

    if (!fs_file_exists(TATR_CONFIG_PATH) || *force) {
        String_Builder cfg = {0};
        sb_append_cstr(&cfg, "# tatr configuration\n");
        bool ok = fs_write_file(TATR_CONFIG_PATH, cfg.items, cfg.count);

        sb_free(cfg);

        if (!ok) {
            log_error("cannot write .tatr/config");
            return 1;
        }
    }

    fs_write_file(TATR_LOG_PATH, "", 0);

    if (!schema_write_current()) {
        log_error("cannot write .tatr/VERSION");
        return 1;
    }

    log_msg("Initialized empty tatr repository in .tatr/");
    return 0;
}
