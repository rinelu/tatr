#ifndef TATR_TEST_HELPERS_H_
#define TATR_TEST_HELPERS_H_

#include "test_framework.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#ifdef _WIN32
#   define WIN32_LEAN_AND_MEAN
#   include <windows.h>
#else
#   include <unistd.h>
#   include <poll.h>
#   include <sys/wait.h>
#endif

#ifndef TATR_BIN
#define TATR_BIN "tatr"
#endif

typedef struct {
    String_Builder *out;
    String_Builder *err;
    int exit_code;
} TatrResult;

#ifdef _WIN32

static inline TatrResult tatr(const char *args)
{
    TatrResult res = {0};
    res.out = tf_sb_new();
    res.err = tf_sb_new();

    String_Builder *cmd = tf_sb_new();
    sb_append_cstr(cmd, TATR_BIN " ");
    sb_append_cstr(cmd, args ? args : "");
    sb_append_null(cmd);

    // Create two anonymous pipes: one for stdout, one for stderr.
    // The child inherits the write ends; we read from the read ends.
    SECURITY_ATTRIBUTES sa = { sizeof(sa), NULL, TRUE };

    HANDLE out_r, out_w, err_r, err_w;
    if (!CreatePipe(&out_r, &out_w, &sa, 0) ||
        !CreatePipe(&err_r, &err_w, &sa, 0)) {
        res.exit_code = -1;
        return res;
    }

    // Make the read ends non-inheritable so the child doesn't hold them open.
    SetHandleInformation(out_r, HANDLE_FLAG_INHERIT, 0);
    SetHandleInformation(err_r, HANDLE_FLAG_INHERIT, 0);

    STARTUPINFOA si = {0};
    si.cb           = sizeof(si);
    si.dwFlags      = STARTF_USESTDHANDLES;
    si.hStdInput    = GetStdHandle(STD_INPUT_HANDLE);
    si.hStdOutput   = out_w;
    si.hStdError    = err_w;

    PROCESS_INFORMATION pi = {0};
    // CreateProcessA needs a mutable cmd buffer.
    char *cmdline = _strdup(cmd->items);
    BOOL ok = CreateProcessA(NULL, cmdline, NULL, NULL, TRUE, 0, NULL, NULL, &si, &pi);
    free(cmdline);

    // Close child's write ends in the parent — required so ReadFile returns
    // EOF when the child exits rather than blocking forever.
    CloseHandle(out_w);
    CloseHandle(err_w);

    if (!ok) {
        CloseHandle(out_r);
        CloseHandle(err_r);
        res.exit_code = -1;
        return res;
    }
    CloseHandle(pi.hThread);

    // Drain both pipes with WaitForMultipleObjects so we don't deadlock when
    // the child fills one pipe's buffer while we're blocked on the other.
    // Use overlapped (async) reads: issue a ReadFile on whichever pipe is
    // ready, accumulate into the builder, repeat until both are closed.
    HANDLE events[2];
    OVERLAPPED ov[2]    = {0};
    char       bufs[2][4096];
    bool       done[2]  = {false, false};
    HANDLE     pipes[2] = {out_r, err_r};

    for (int i = 0; i < 2; i++) {
        events[i] = CreateEventA(NULL, TRUE, FALSE, NULL);
        ov[i].hEvent = events[i];
    }

    // Kick off the first async read on each pipe.
    for (int i = 0; i < 2; i++) {
        DWORD n = 0;
        if (!ReadFile(pipes[i], bufs[i], sizeof bufs[i], &n, &ov[i])) {
            if (GetLastError() == ERROR_BROKEN_PIPE) done[i] = true;
            // ERROR_IO_PENDING is expected for overlapped I/O — do nothing.
        } else {
            // Completed synchronously.
            String_Builder *sb = (i == 0) ? res.out : res.err;
            sb_append_buf(sb, bufs[i], n);
            ResetEvent(events[i]);
            // Re-issue.
            ReadFile(pipes[i], bufs[i], sizeof bufs[i], &n, &ov[i]);
        }
    }

    while (!done[0] || !done[1]) {
        // Build a list of events still pending.
        HANDLE wait_ev[2];
        int    wait_idx[2];
        DWORD  wait_count = 0;
        for (int i = 0; i < 2; i++) {
            if (!done[i]) {
                wait_ev[wait_count]  = events[i];
                wait_idx[wait_count] = i;
                wait_count++;
            }
        }

        DWORD w = WaitForMultipleObjects(wait_count, wait_ev, FALSE, INFINITE);
        if (w == WAIT_FAILED) break;

        int i = wait_idx[w - WAIT_OBJECT_0];
        DWORD n = 0;
        if (GetOverlappedResult(pipes[i], &ov[i], &n, FALSE)) {
            String_Builder *sb = (i == 0) ? res.out : res.err;
            sb_append_buf(sb, bufs[i], n);
            ResetEvent(events[i]);
            // Re-issue the next read.
            if (!ReadFile(pipes[i], bufs[i], sizeof bufs[i], &n, &ov[i])) {
                if (GetLastError() == ERROR_BROKEN_PIPE) done[i] = true;
            } else {
                // Completed synchronously again.
                sb_append_buf(sb, bufs[i], n);
                ResetEvent(events[i]);
                ReadFile(pipes[i], bufs[i], sizeof bufs[i], &n, &ov[i]);
            }
        } else {
            if (GetLastError() == ERROR_BROKEN_PIPE) done[i] = true;
        }
    }

    for (int i = 0; i < 2; i++) {
        CloseHandle(events[i]);
        CloseHandle(pipes[i]);
    }

    sb_append_null(res.out); res.out->count--;
    sb_append_null(res.err); res.err->count--;

    WaitForSingleObject(pi.hProcess, INFINITE);
    DWORD ec = 0;
    GetExitCodeProcess(pi.hProcess, &ec);
    CloseHandle(pi.hProcess);
    res.exit_code = (int)ec;

    return res;
}

#else  // POSIX

static inline TatrResult tatr(const char *args)
{
    TatrResult res = {0};
    res.out = tf_sb_new();
    res.err = tf_sb_new();

    String_Builder *cmd = tf_sb_new();
    sb_append_cstr(cmd, TATR_BIN " ");
    sb_append_cstr(cmd, args ? args : "");
    sb_append_null(cmd);

    int out_pipe[2], err_pipe[2];
    if (pipe(out_pipe) < 0 || pipe(err_pipe) < 0) {
        res.exit_code = -1;
        return res;
    }

    pid_t pid = fork();
    if (pid < 0) {
        close(out_pipe[0]); close(out_pipe[1]);
        close(err_pipe[0]); close(err_pipe[1]);
        res.exit_code = -1;
        return res;
    }

    if (pid == 0) {
        dup2(out_pipe[1], STDOUT_FILENO);
        dup2(err_pipe[1], STDERR_FILENO);
        close(out_pipe[0]); close(out_pipe[1]);
        close(err_pipe[0]); close(err_pipe[1]);
        execl("/bin/sh", "sh", "-c", cmd->items, (char *)NULL);
        _exit(127);
    }

    close(out_pipe[1]);
    close(err_pipe[1]);

    struct pollfd pfds[2] = {
        { .fd = out_pipe[0], .events = POLLIN },
        { .fd = err_pipe[0], .events = POLLIN },
    };

    char chunk[4096];
    int open_pipes = 2;

    while (open_pipes > 0) {
        int ready = poll(pfds, 2, -1);
        if (ready < 0) break;

        for (int i = 0; i < 2; i++) {
            if (pfds[i].fd < 0) continue;

            if (pfds[i].revents & POLLIN) {
                ssize_t n = read(pfds[i].fd, chunk, sizeof chunk);
                if (n > 0) {
                    String_Builder *sb = (i == 0) ? res.out : res.err;
                    sb_append_buf(sb, chunk, (size_t)n);
                    continue;
                }
            }

            if (pfds[i].revents & (POLLHUP | POLLERR | POLLNVAL)) {
                ssize_t n;
                String_Builder *sb = (i == 0) ? res.out : res.err;
                while ((n = read(pfds[i].fd, chunk, sizeof chunk)) > 0)
                    sb_append_buf(sb, chunk, (size_t)n);
                close(pfds[i].fd);
                pfds[i].fd = -1;
                open_pipes--;
            }
        }
    }

    sb_append_null(res.out); res.out->count--;
    sb_append_null(res.err); res.err->count--;

    int status;
    waitpid(pid, &status, 0);
    res.exit_code = WIFEXITED(status) ? WEXITSTATUS(status) : -1;

    return res;
}

#endif // _WIN32

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static inline const char *tatr_combined(TatrResult *r)
{
    String_Builder *sb = tf_sb_new();
    sb_append_buf(sb, r->out->items, r->out->count);
    sb_append_buf(sb, r->err->items, r->err->count);
    sb_append_null(sb);
    return sb->items;
}

#define tatr_init() \
    do { \
        TatrResult _ri = tatr("init"); \
        if (_ri.exit_code != 0) \
            TF_FAIL_MSG("tatr init failed (exit %d):\n  out: %s\n  err: %s", \
                        _ri.exit_code, _ri.out->items, _ri.err->items); \
    } while(0)

static inline bool tatr_new_issue(const char *extra, char *out_id, size_t len)
{
    String_Builder *cmd = tf_sb_new();
    sb_append_cstr(cmd, "new");
    if (extra && *extra) {
        sb_append_cstr(cmd, " ");
        sb_append_cstr(cmd, extra);
    }
    sb_append_null(cmd);

    TatrResult r = tatr(cmd->items);
    if (r.exit_code != 0) return false;

    const char *pfx = "Created issue ";
    char *p = strstr(r.out->items, pfx);
    if (!p) p = strstr(r.err->items, pfx);
    if (!p) return false;

    p += strlen(pfx);
    size_t i = 0;
    while (*p && *p != '\n' && *p != '\r' && *p != ' ' && i < len - 1)
        out_id[i++] = *p++;
    out_id[i] = '\0';

    return i > 0;}

static inline bool tf_create_raw_issue(const char *id,     const char *title,
                                        const char *status, const char *priority,
                                        const char *tags,   const char *body)
{
    const char *dir  = fs_path(".tatr/issues", id);
    const char *att  = fs_path(".tatr/issues", id, "attachments");
    const char *path = fs_path(".tatr/issues", id, "issue.tatr");

    if (!fs_mkdir_force(dir, true)) return false;
    if (!fs_mkdir_force(att, true)) return false;

    String_Builder *sb = tf_sb_new();
    sb_appendf(sb, "title: %s\n",    title    ? title    : "");
    sb_appendf(sb, "status: %s\n",   status   ? status   : "open");
    sb_appendf(sb, "priority: %s\n", priority ? priority : "normal");
    sb_appendf(sb, "created: 2024-01-15T10:00:00\n");
    sb_appendf(sb, "tags: %s\n",     tags     ? tags     : "");
    sb_appendf(sb, "---\n%s\n",      body     ? body     : "");
    sb_append_null(sb);

    return fs_write_file(path, sb->items, sb->count - 1);
}

#define ASSERT_CMD_OK(r) \
    do { if ((r).exit_code != 0) \
        TF_FAIL_MSG("command failed (exit %d):\n  out: %s\n  err: %s", \
                    (r).exit_code, (r).out->items, (r).err->items); } while(0)

#define ASSERT_CMD_FAIL(r) \
    do { if ((r).exit_code == 0) \
        TF_FAIL_MSG("command unexpectedly succeeded:\n  out: %s", \
                    (r).out->items); } while(0)

#define ASSERT_OUT_CONTAINS(r, n)     ASSERT_CONTAINS(tatr_combined(&(r)), n)
#define ASSERT_OUT_NOT_CONTAINS(r, n) ASSERT_NOT_CONTAINS(tatr_combined(&(r)), n)

#endif // TATR_TEST_HELPERS_H_
