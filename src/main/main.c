#include "global.h"
#include <stdbool.h>

#define PROGRAM_NAME "tatr: "
#define LOG_IMPLEMENTATION
#include "log.h"

#define CLAG_IMPLEMENTATION
#define NEED_CMD_HELPER
#include "cmd.h"
#include "module.h"

char g_repo_root[PATH_MAX] = {0};

static bool tatr_find_root(void)
{
    char cwd[4096];
    if (!getcwd(cwd, sizeof(cwd))) return false;
 
    char probe[4096 + 16];
    while (1) {
        // Build probe path
        snprintf(probe, sizeof(probe), "%s" FS_SEP ".tatr", cwd);
 
        // follows symlinks
        if (fs_get_file_type(probe) == FST_DIRECTORY) {
            snprintf(g_repo_root, sizeof(g_repo_root), "%s", cwd);
            return true;
        }
 
        char *last = strrchr(cwd, '/');
#ifdef _WIN32
        char *last2 = strrchr(cwd, '\\');
        if (last2 > last) last = last2;
        if (!last || last == cwd + 2) break;
#else
        // filesystem root
        if (!last || last == cwd) break;
#endif
        *last = '\0';
    }
 
    return false;
}

int main(int argc, char **argv)
{
    log_init();
    config_write_global_default();

    if (tatr_modules_init() != TATR_OK) {
        log_error("a compiled-in module failed to initialize");
        return 1;
    }

    if (argc < 2) {
        print_global_help();
        return 0;
    }

    const char *cmd_name = argv[1];
    int sub_argc         = argc - 1;
    char **sub_argv      = argv + 1;

    const Command *cmd = find_command(cmd_name);
    if (!cmd) {
        log_error("unknown command '%s'", cmd_name);
        log_info("Run `tatr help` for a list of commands.");
        return 1;
    }

    tatr_find_root();
    clag_reset();
    return cmd->fn(sub_argc, sub_argv);
}
