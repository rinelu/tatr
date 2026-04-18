#include "editor.h"
#include "fs.h"
#include "ui.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <spawn.h>
#include <sys/wait.h>
extern char **environ;
#endif

static int split_args(const char *cmd, char **argv, int max_args)
{
    int argc = 0;

    while (*cmd && argc < max_args - 1) {
        while (*cmd == ' ') cmd++;
        if (!*cmd) break;

        argv[argc++] = (char *)cmd;

        while (*cmd && *cmd != ' ') cmd++;
        if (*cmd) {
            *(char *)cmd = '\0';
            cmd++;
        }
    }

    argv[argc] = NULL;
    return argc;
}

static int run_editor(const char *editor, const char *path)
{
    char buf[256];
    snprintf(buf, sizeof(buf), "%s", editor);
    char *argv[32];
    int argc = split_args(buf, argv, 32);

    if (argc == 0) return -1;

    argv[argc++] = (char *)path;
    argv[argc] = NULL;

#ifdef _WIN32
    char cmdline[512] = {0};
    for (int i = 0; argv[i]; i++) {
        if (i > 0) strcat(cmdline, " ");
        strcat(cmdline, argv[i]);
    }

    STARTUPINFOA si = {0};
    PROCESS_INFORMATION pi = {0};
    si.cb = sizeof(si);

    if (!CreateProcessA(NULL, cmdline, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi))
        return -1;

    WaitForSingleObject(pi.hProcess, INFINITE);
    DWORD exit_code = 0;
    GetExitCodeProcess(pi.hProcess, &exit_code);

    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    return (int)exit_code;
#else

    pid_t pid;
    int status;

    if (posix_spawnp(&pid, argv[0], NULL, NULL, argv, environ) != 0)
        return -1;
    if (waitpid(pid, &status, 0) < 0)
        return -1;

    if (WIFEXITED(status))
        return WEXITSTATUS(status);

    return -1;
#endif
}

static const char *get_editor(void)
{
    const char *ed = getenv("VISUAL");
    if (!ed || !*ed) ed = getenv("EDITOR");
    if (!ed || !*ed) ed = "vi";
    return ed;
}

bool editor_edit(const char *content, size_t content_len, const char *suffix, String_Builder *out_sb)
{
    char tmp_path[256];

#ifdef _WIN32
    char tmp_dir[MAX_PATH];
    GetTempPathA(sizeof(tmp_dir), tmp_dir);
    snprintf(tmp_path, sizeof(tmp_path), "%statr_editor_XXXXXX%s", tmp_dir, suffix ? suffix : "");
#else
    snprintf(tmp_path, sizeof(tmp_path), "/tmp/tatr_editor_XXXXXX%s", suffix ? suffix : "");
#endif

#ifdef _WIN32
    if (_mktemp_s(tmp_path, sizeof(tmp_path)) != 0) {
        log_error("cannot create temp file");
        return false;
    }
    FILE *f = fopen(tmp_path, "wb");
#else
    int suffix_len = suffix ? (int)strlen(suffix) : 0;
    int fd = mkstemps(tmp_path, suffix_len);
    if (fd < 0) {
        log_error("cannot create temp file");
        return false;
    }
    FILE *f = fdopen(fd, "wb");
#endif

    if (!f) {
        log_error("cannot open temp file '%s'", tmp_path);
        return false;
    }

    fwrite(content, 1, content_len, f);
    fclose(f);

    // run editor
    String_Builder cmd = {0};
    sb_appendf(&cmd, "%s %s", get_editor(), tmp_path);
    sb_append_null(&cmd);

    int rc = run_editor(get_editor(), tmp_path);
    sb_free(cmd);

    if (rc != 0) {
        log_error("editor exited with code %d", rc);
        fs_delete_file(tmp_path);
        return false;
    }

    // read result
    bool ok = fs_read_file(tmp_path, out_sb);
    fs_delete_file(tmp_path);

    if (!ok) {
        log_error("cannot read temp file after editing");
        return false;
    }

    return true;
}
