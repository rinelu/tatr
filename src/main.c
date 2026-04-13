#define FS_SILENT

#define LOG_GLOBAL
#define LOG_IMPLEMENTATION
#include "log.h"

#include <stdbool.h>

#define CLAG_IMPLEMENTATION
#define NEED_CMD_HELPER
#include "cmd/cmd.h"

#include "ui.h"

int main(int argc, char **argv)
{
    if (argc < 2) {
        print_global_help();
        return 0;
    }

    ui_init( .show_header = true );

    const char *cmd_name = argv[1];

    int sub_argc    = argc - 1;
    char **sub_argv = argv + 1;

    const Command *cmd = find_command(cmd_name);
    if (!cmd) {
        log_error("tatr: unknown command '%s'", cmd_name);
        log_info("Run `tatr help` for a list of commands.");
        return 1;
    }

    clag_reset();
    int result = cmd->fn(sub_argc, sub_argv);
    // int result = cmd->fn(sub_argc, sub_argv) ? 1 : 0;
    return result;
}
