#ifndef CMD_H_
#define CMD_H_

#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <string.h>

#include "fs.h"
#include "global.h"
#include "clag.h"
#include "issue.h"
#include "ui.h"
#include "temp.h"

inline static bool require_repo(void)
{
    if (!fs_file_exists(".tatr/issues")) {
        ui_error("tatr: not a tatr repository (run `tatr init` first)");
        return false;
    }
    return true;
}

static int cmd_version(int argc, char **argv);
static int cmd_help   (int argc, char **argv);
int cmd_init   (int argc, char **argv);
int cmd_new    (int argc, char **argv);
int cmd_list   (int argc, char **argv);
int cmd_show   (int argc, char **argv);
int cmd_edit   (int argc, char **argv);
int cmd_comment(int argc, char **argv);
int cmd_search (int argc, char **argv);
int cmd_tag    (int argc, char **argv);
int cmd_close  (int argc, char **argv);
int cmd_delete (int argc, char **argv);
int cmd_reopen (int argc, char **argv);
int cmd_attach (int argc, char **argv);
int cmd_attachls(int argc, char **argv);
int cmd_detach (int argc, char **argv);
int cmd_status (int argc, char **argv);

typedef int (*CmdFn)(int argc, char **argv);

typedef struct {
    const char *name;
    const char *summary;
    CmdFn fn;
} Command;

static const Command commands[] = {
    { "Core", NULL, NULL },
    { "version",  "Print tatr version",                                     cmd_version  },
    { "help",     "Print help for tatr or a specific command",              cmd_help     },
    { "init",     "Initialize a .tatr directory in the current folder",     cmd_init     },
    { "status",   "Show repository status",                                 cmd_status   },

    { "Issues", NULL, NULL },
    { "new",      "Create a new issue",                                     cmd_new      },
    { "list",     "List issues (filter by status, priority, tag)",          cmd_list     },
    { "show",     "Show a single issue in full",                            cmd_show     },
    { "edit",     "Edit a field (title, status, priority, tags) in-place",  cmd_edit     },
    { "delete",   "Permanently delete an issue and its attachments",        cmd_delete   },
    { "close",    "Mark an issue as closed",                                cmd_close    },
    { "reopen",   "Reopen a closed issue",                                  cmd_reopen   },

    { "Organization", NULL, NULL },
    { "search",   "Full-text search across all issue files",                cmd_search   },
    { "tag",      "Add or remove tags on an issue",                         cmd_tag      },

    { "Collaboration", NULL, NULL },
    { "comment",  "Append a comment block to an issue's body",              cmd_comment  },
    { "attach",   "Copy a file into an issue's attachments/",               cmd_attach   },
    { "attachls", "List all attachments of an issue",                       cmd_attachls },
    { "detach",   "Remove an attachment file from an issue",                cmd_detach   },
};

static const int COMMANDS_COUNT = (int)(sizeof(commands) / sizeof(*commands));

static const Command *find_command(const char *name)
{
    for (int i = 0; i < COMMANDS_COUNT; i++)
        if (strcmp(commands[i].name, name) == 0 && commands[i].summary != NULL)
            return &commands[i];
    return NULL;
}

static void print_global_help(void)
{
    ui_msg("");
    ui_msg("tatr %s - tiny issue tracker\n", TATR_VERSION);
    ui_msg("Usage: tatr <command> [options]\n");
    ui_msg("Commands:");
    for (int i = 0; i < COMMANDS_COUNT; i++) {
        if (commands[i].summary == NULL) {
            ui_msg("\n  %s", commands[i].name);
            continue;
        }
        ui_msg("    %-12s  %s", commands[i].name, commands[i].summary);
    }
    ui_msg("\nRun `tatr help <command>` for per-command help.\n");
}

static int cmd_version(int argc, char **argv)
{
    (void)argc; (void)argv;
    ui_info("tatr %s", TATR_VERSION);
    return 0;
}

static int cmd_help(int argc, char **argv)
{
    if (argc <= 1) {
        print_global_help();
        return 0;
    }

    const Command *c = find_command(argv[1]);
    if (!c) {
        ui_error("tatr: unknown command '%s'", argv[1]);
        return 1;
    }
    char *help_argv[] = { (char *)c->name, "--help", NULL };
    return c->fn(2, help_argv);
}

#endif // CMD_H_
