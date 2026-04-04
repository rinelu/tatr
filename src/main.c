#include <stdbool.h>
#include "cmd/cmd.h"
#include "ui.h"

#define CLAG_IMPLEMENTATION
#include "clag.h"

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
        ui_error("tatr: unknown command '%s'", cmd_name);
        ui_info("Run `tatr help` for a list of commands.");
        return 1;
    }

    int result = cmd->fn(sub_argc, sub_argv);
    // int result = cmd->fn(sub_argc, sub_argv) ? 1 : 0;
    return result;
}
