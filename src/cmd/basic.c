#define NEED_CMD_HELPER
#include "cmd.h"

int cmd_help(int argc, char **argv)
{
    if (argc <= 1) {
        print_global_help();
        return 0;
    }

    const Command *c = find_command(argv[1]);
    if (!c) {
        log_error("tatr: unknown command '%s'", argv[1]);
        return 1;
    }
    char *help_argv[] = { (char *)c->name, "--help", NULL };
    return c->fn(2, help_argv);
}

int cmd_version(int argc, char **argv)
{
    (void)argc; (void)argv;
    log_info("tatr %s", TATR_VERSION);
    return 0;
}
