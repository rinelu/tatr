#include "cmd.h"
#include "fs.h"
#include "global.h"

int cmd_init(int argc, char **argv)
{
    bool *force = clag_bool("force", 'f', false, "reinitialize even if .tatr already exists");
    clag_usage("[options]");

    if (!clag_parse(argc, argv)) {
        clag_print_error(stderr);
        return 1;
    }

    if (fs_file_exists(".tatr") && !*force) {
        log_error("tatr: .tatr already exists (use --force or -f to reinitialize)");
        return 1;
    }

    fs_mkdir(".tatr");
    if (!fs_mkdir_force(".tatr/issues", force)) {
        log_error("tatr: cannot create .tatr/issues");
        return 1;
    }

    if (!fs_file_exists(".tatr/config") || *force) {
        String_Builder cfg = {0};
        sb_append_cstr(&cfg, "# tatr configuration\n");
        sb_append_cstr(&cfg, "default_status: open\n");
        sb_append_cstr(&cfg, "default_priority: normal\n");
        bool ok = fs_write_file(".tatr/config", cfg.items, cfg.count);

        sb_free(cfg);

        if (!ok) {
            log_error("tatr: cannot write .tatr/config");
            return 1;
        }
    }

    fs_write_file(TATRLOG_PATH, "", 0);
    log_msg("Initialized empty tatr repository in .tatr/");
    return 0;
}
