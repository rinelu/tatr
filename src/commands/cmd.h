#ifndef CMD_H_
#define CMD_H_

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "fs.h"
#include "global.h"
#include "clag.h"
#include "issue.h"
#include "temp.h"
#include "ui.h"
#include "log.h"
#include "config.h"
#include "tatrlog.h"
#include "schema.h"

inline static bool require_repo(void)
{
    if (g_repo_root[0] == '\0') {
        log_error("not a tatr repository (run `tatr init` first)");
        return false;
    }

    if (schema_check_and_migrate() != TATR_OK)
        return false;

    return true;
}

int cmd_version(int argc, char **argv);
int cmd_help   (int argc, char **argv);
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
int cmd_detach  (int argc, char **argv);
int cmd_status  (int argc, char **argv);

#ifdef TATR_MODULE_EXPORT
#include "export.h"
int cmd_export  (int argc, char **argv);
#endif

int cmd_log(int argc, char **argv);
int cmd_config(int argc, char **argv);
typedef int (*CmdFn)(int argc, char **argv);

// A command with no flags set is assumed to mutate the repo unless CMD_CAP_READONLY is present.
typedef enum {
    CMD_CAP_READONLY = 1u << 0, // never writes to the repo              
    CMD_CAP_MUTATES  = 1u << 1, // creates/modifies/deletes repo state   
} Cmd_Capabilities;

typedef struct {
    const char *name;
    const char *summary;
    CmdFn fn;
    unsigned capabilities;
} Command;

#define GROUP(name) {name,NULL,NULL,0},
static const Command commands[] = {
         GROUP("Core")
     { "version",  "Print tatr version",                                     cmd_version,  CMD_CAP_READONLY },
     { "help",     "Print help for tatr or a specific command",              cmd_help,     CMD_CAP_READONLY },
     { "init",     "Initialize a .tatr directory in the current folder",     cmd_init,     CMD_CAP_MUTATES  },
     { "status",   "Show repository status",                                 cmd_status,   CMD_CAP_READONLY },
     { "config",   "Get and set repository or global options",               cmd_config,   CMD_CAP_MUTATES  },

         GROUP("Issues")
     { "new",      "Create a new issue",                                     cmd_new,      CMD_CAP_MUTATES  },
     { "list",     "List issues (filter by status, priority, tag)",          cmd_list,     CMD_CAP_READONLY },
     { "show",     "Show a single issue in full",                            cmd_show,     CMD_CAP_READONLY },
    { "edit",     "Edit a field (title, status, priority, tags) in-place",  cmd_edit,     CMD_CAP_MUTATES  },
    { "delete",   "Permanently delete an issue and its attachments",        cmd_delete,   CMD_CAP_MUTATES  },
    { "close",    "Mark an issue as closed",                                cmd_close,    CMD_CAP_MUTATES  },
    { "reopen",   "Reopen a closed issue",                                  cmd_reopen,   CMD_CAP_MUTATES  },
#ifdef TATR_MODULE_EXPORT
    { "export",   "Export an issue to Markdown or JSON",                    cmd_export,   CMD_CAP_READONLY },
#endif
    { "log",      "Show history of changes",                                cmd_log,      CMD_CAP_READONLY },

         GROUP("Organizaton")
    { "search",   "Full-text search across all issue files",                cmd_search,   CMD_CAP_READONLY },
    { "tag",      "Add or remove tags on an issue",                         cmd_tag,      CMD_CAP_MUTATES  },

         GROUP("Collaboration")
    { "comment",  "Append a comment block to an issue's body",              cmd_comment,  CMD_CAP_MUTATES  },
    { "attach",   "Copy a file into an issue's attachments/",               cmd_attach,   CMD_CAP_MUTATES  },
    { "attachls", "List all attachments of an issue",                       cmd_attachls, CMD_CAP_READONLY },
    { "detach",   "Remove an attachment file from an issue",                cmd_detach,   CMD_CAP_MUTATES  },
};

static const int COMMANDS_COUNT = (int)(sizeof(commands) / sizeof(*commands));

#ifdef NEED_CMD_HELPER
static void print_global_help(void)
{
    log_msg("\ntatr %s - tiny issue(s) tracker\n", TATR_VERSION);
    log_msg("Usage: tatr <command> [options]\n");
    log_msg("Commands:");
    for (int i = 0; i < COMMANDS_COUNT; i++) {
        if (commands[i].summary == NULL) {
            log_msg("\n  %s", commands[i].name);
            continue;
        }
        log_msg("    %-12s  %s", commands[i].name, commands[i].summary);
    }
    log_msg("\nRun `tatr help <command>` for per-command help.\n");
}

static const Command *find_command(const char *name)
{
    for (int i = 0; i < COMMANDS_COUNT; i++)
        if (strcmp(commands[i].name, name) == 0 && commands[i].summary != NULL)
            return &commands[i];
    return NULL;
}
#endif // NEED_CMD_HELPER

#endif // CMD_H_
