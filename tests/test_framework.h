#ifndef TATR_TEST_FRAMEWORK_H_
#define TATR_TEST_FRAMEWORK_H_

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <signal.h>
#include <assert.h>
#include <errno.h>

#ifdef _WIN32
#   define WIN32_LEAN_AND_MEAN
#   include <windows.h>
#   include <direct.h>     // _mkdir, _chdir
#   include <io.h>
#   include <process.h>    // _getpid
#   ifdef _MSC_VER
        typedef int pid_t;
#   endif
#   define TF_WIN32 1
#else
#   include <unistd.h>
#   include <sys/stat.h>
#   include <sys/types.h>
#   include <sys/wait.h>
#   define TF_WIN32 0
#endif

#include "../libs/fs.h"
#include "../libs/astring.h"
#include "../libs/temp.h"
#include "../libs/array.h"

#define LOG_GLOBAL
#include "../libs/log.h"

#define TF_RED    "\x1b[31m"
#define TF_GREEN  "\x1b[32m"
#define TF_YELLOW "\x1b[33m"
#define TF_CYAN   "\x1b[36m"
#define TF_BOLD   "\x1b[1m"
#define TF_DIM    "\x1b[2m"
#define TF_RESET  "\x1b[0m"

static inline int tf__chdir(const char *path)
{
#ifdef _WIN32
    return _chdir(path);
#else
    return chdir(path);
#endif
}

static inline bool tf__mkdtemp(char *out, size_t outsz)
{
#ifdef _WIN32
    char tmp[MAX_PATH];
    if (!GetTempPathA((DWORD)sizeof(tmp), tmp)) return false;

    // Try up to 1000 unique names before giving up.
    for (int i = 0; i < 1000; i++) {
        char name[MAX_PATH];
        snprintf(name, sizeof name, "%statr_test_%d_%d", tmp, (int)GetCurrentProcessId(), i);
        if (CreateDirectoryA(name, NULL)) {
            snprintf(out, outsz, "%s", name);
            return true;
        }
        if (GetLastError() != ERROR_ALREADY_EXISTS) return false;
    }
    return false;
#else
    (void)outsz;
    return mkdtemp(out) != NULL;
#endif
}

typedef struct {
    String_Builder **items;
    size_t count;
    size_t capacity;
} TF_SB_Registry;

extern TF_SB_Registry _tf_sbs;

static inline String_Builder *tf_sb_new(void)
{
    String_Builder *sb = calloc(1, sizeof(*sb));
    assert(sb && "tf_sb_new: not enough memory");
    da_append(&_tf_sbs, sb);
    return sb;
}

static inline void tf_sb_free_all(void)
{
    da_foreach(String_Builder *, sb, &_tf_sbs) {
        sb_free(**sb);
        free(*sb);
    }
    da_free(_tf_sbs);
    _tf_sbs.items    = NULL;
    _tf_sbs.count    = 0;
    _tf_sbs.capacity = 0;
}

// ---------------------------------------------------------------------------
// Test state
// ---------------------------------------------------------------------------

typedef struct {
    int         tests_run;
    int         tests_passed;
    int         tests_failed;
    int         suites_run;
    const char *current_suite;
    char        sandbox[256];
} TF_State;

extern TF_State _tf;
extern bool     delete_fail_tmp;

// ---------------------------------------------------------------------------
// Sandbox helpers
// ---------------------------------------------------------------------------

static inline void tf__safe_name(const char *name, char *out, size_t sz)
{
    snprintf(out, sz, "%s", name);
    for (char *p = out; *p; p++)
        if (*p == ':' || *p == ' ' || *p == '/' || *p == '\\')
            *p = '_';
}

static inline bool tf_sandbox_enter(const char *name)
{
    char safe[128];
    tf__safe_name(name, safe, sizeof safe);

#ifdef _WIN32
    (void)safe;
    _tf.sandbox[0] = '\0';
#else
    snprintf(_tf.sandbox, sizeof _tf.sandbox, "/tmp/tatr_%s_XXXXXX", safe);
#endif

    if (!tf__mkdtemp(_tf.sandbox, sizeof _tf.sandbox)) return false;
    return tf__chdir(_tf.sandbox) == 0;
}

static inline void tf_sandbox_exit(bool delete)
{
    tf__chdir(
#ifdef _WIN32
        "C:\\"
#else
        "/tmp"
#endif
    );
    if (_tf.sandbox[0]) {
        if (delete) fs_delete_recursive(_tf.sandbox);
        _tf.sandbox[0] = '\0';
    }
    tf_sb_free_all();
    temp_reset();
}

// ---------------------------------------------------------------------------
// File / path helpers
// ---------------------------------------------------------------------------

static inline bool tf_write_file(const char *path, const char *content)
{
    return fs_write_file(path, content, strlen(content));
}

static inline const char *tf_read_file(const char *path)
{
    String_Builder *sb = tf_sb_new();
    if (!fs_read_file(path, sb)) return NULL;
    sb_append_null(sb);
    sb->count--;
    return sb->items;
}

static inline bool tf_path_exists(const char *path) { return fs_file_exists(path); }
static inline bool tf_mkdir(const char *path)       { return fs_mkdir_force(path, true); }

// ---------------------------------------------------------------------------
// Assertion macros
// ---------------------------------------------------------------------------

#define TF_FAIL_MSG(fmt, ...) \
    do { \
        log_msg(TF_RED "[ FAIL ]" TF_RESET " %s:%d  " fmt, \
                __FILE__, __LINE__, ##__VA_ARGS__); \
        _tf.tests_failed++; return; \
    } while(0)

#define ASSERT_TRUE(e)  do { if (!(e)) TF_FAIL_MSG("expected true: %s",  #e); } while(0)
#define ASSERT_FALSE(e) do { if ( (e)) TF_FAIL_MSG("expected false: %s", #e); } while(0)

#define ASSERT_EQ_INT(a, b) \
    do { int _a = (int)(a), _b = (int)(b); \
         if (_a != _b) TF_FAIL_MSG("%s==%s -> %d!=%d", #a, #b, _a, _b); } while(0)

#define ASSERT_NE_INT(a, b) \
    do { int _a = (int)(a), _b = (int)(b); \
         if (_a == _b) TF_FAIL_MSG("%s!=%s -> both %d", #a, #b, _a); } while(0)

#define ASSERT_EQ_STR(a, b) \
    do { const char *_a = (a), *_b = (b); \
         if (!_a || !_b || strcmp(_a, _b)) \
             TF_FAIL_MSG("\"%s\" != \"%s\"", _a ? _a : "(null)", _b ? _b : "(null)"); } while(0)

#define ASSERT_CONTAINS(hay, needle) \
    do { const char *_h = (hay), *_n = (needle); \
         if (!_h || !_n || !strstr(_h, _n)) \
             TF_FAIL_MSG("expected string to contain \"%s\"", _n ? _n : "(null)"); } while(0)

#define ASSERT_NOT_CONTAINS(hay, needle) \
    do { const char *_h = (hay), *_n = (needle); \
         if (_h && _n && strstr(_h, _n)) \
             TF_FAIL_MSG("expected string NOT to contain \"%s\"", _n); } while(0)

#define ASSERT_PATH_EXISTS(p) \
    do { if (!tf_path_exists(p)) TF_FAIL_MSG("path missing: %s", p); } while(0)

#define ASSERT_PATH_NOT_EXISTS(p) \
    do { if (tf_path_exists(p))  TF_FAIL_MSG("path should not exist: %s", p); } while(0)

#define ASSERT_NULL(p)     do { if ((p) != NULL) TF_FAIL_MSG("expected NULL: %s",     #p); } while(0)
#define ASSERT_NOT_NULL(p) do { if ((p) == NULL) TF_FAIL_MSG("expected non-NULL: %s", #p); } while(0)

// ---------------------------------------------------------------------------
// Test registration
// ---------------------------------------------------------------------------

typedef void (*TF_TestFn)(void);

typedef struct {
    const char *name;
    const char *suite;
    TF_TestFn   fn;
    TF_TestFn   setup;
    TF_TestFn   teardown;
} TF_TestCase;

typedef struct {
    TF_TestCase *items;
    size_t       count;
    size_t       capacity;
} TF_TestCases;

extern TF_TestCases _tf_tests;

#define SUITE(name) do { _tf.suites_run++; _tf.current_suite = (name); } while(0)
#define RUN_TEST(fn)                    tf__register_test(#fn, fn, NULL,  NULL)
#define RUN_TEST_F(fn, setup, teardown) tf__register_test(#fn, fn, setup, teardown)

static inline void tf__register_test(const char *name, TF_TestFn fn, TF_TestFn setup, TF_TestFn teardown)
{
    da_append(&_tf_tests, ((TF_TestCase){
        .name     = name,
        .suite    = _tf.current_suite,
        .fn       = fn,
        .setup    = setup,
        .teardown = teardown,
    }));
}

extern volatile sig_atomic_t tf_interrupted;

static void tf__handle_sigint(int sig) { (void)sig; tf_interrupted = 1; }

static inline void tf_install_signal_handler(void)
{
#ifdef _WIN32
    signal(SIGINT, tf__handle_sigint);
#else
    struct sigaction sa = {0};
    sa.sa_handler = tf__handle_sigint;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGINT, &sa, NULL);
#endif
}

// Shared single-test runner used by both paths.
static inline void tf__run_one(TF_TestCase *t, const char *sandbox_path)
{
    snprintf(_tf.sandbox, sizeof _tf.sandbox, "%s", sandbox_path);
    if (tf__chdir(_tf.sandbox) != 0) {
        log_msg(TF_RED "[ ERROR ]" TF_RESET " %s: chdir into sandbox failed", t->name);
        return;
    }

    int prev = _tf.tests_failed;
    if (t->setup)                    t->setup();
    if (_tf.tests_failed == prev)    t->fn();
    if (t->teardown)                 t->teardown();

    bool passed = (_tf.tests_failed == prev);
    if (passed)
        log_msg(TF_GREEN "[ SUCCESS ]" TF_RESET " [%s] %s", t->suite, t->name);
    else
        log_msg(TF_RED   "[ FAIL ]"    TF_RESET " [%s] %s", t->suite, t->name);

    tf_sandbox_exit(passed || delete_fail_tmp);
}

#ifdef _WIN32

static inline void tf_run_all_parallel(int max_workers)
{
    (void)max_workers;
    log_msg(TF_YELLOW "[ note ]" TF_RESET " Parallel execution not supported on Windows - running sequentially.");

    int test_count = (int)_tf_tests.count;
    for (int i = 0; i < test_count && !tf_interrupted; i++) {
        TF_TestCase *t = &_tf_tests.items[i];

        char safe[128];
        tf__safe_name(t->name, safe, sizeof safe);
        char sandbox[256] = {0};
        if (!tf__mkdtemp(sandbox, sizeof sandbox)) {
            log_msg(TF_RED "[ ERROR ]" TF_RESET " %s: mkdtemp failed", t->name);
            _tf.tests_failed++;
            _tf.tests_run++;
            continue;
        }

        _tf.tests_run++;
        int prev = _tf.tests_failed;
        tf__run_one(t, sandbox);
        if (_tf.tests_failed == prev)
            _tf.tests_passed++;
        else
            _tf.tests_failed++;  // already incremented inside assertion, don't double-count
    }

    if (tf_interrupted)
        log_msg(TF_YELLOW "\nInterrupted." TF_RESET);
}

#else

static inline void tf__kill_and_reap(pid_t *pids, char **sandbox_paths, int count)
{
    for (int i = 0; i < count; i++)
        if (pids[i] > 0) kill(pids[i], SIGTERM);

    int remaining = count;
    while (remaining > 0) {
        int status;
        pid_t done = waitpid(-1, &status, 0);
        if (done <= 0) break;
        remaining--;
    }

    for (int i = 0; i < count; i++) {
        if (sandbox_paths[i]) {
            if (fs_file_exists(sandbox_paths[i]))
                fs_delete_recursive(sandbox_paths[i]);
            free(sandbox_paths[i]);
            sandbox_paths[i] = NULL;
        }
    }
}

static inline void tf__run_child(TF_TestCase *t, const char *sandbox_path)
{
    int prev = _tf.tests_failed;
    tf__run_one(t, sandbox_path);
    bool passed = (_tf.tests_failed == prev);
    exit(passed ? 0 : 1);
}

static inline void tf_run_all_parallel(int max_workers)
{
    pid_t  *pids          = calloc((size_t)max_workers, sizeof(pid_t));
    char  **sandbox_paths = calloc((size_t)max_workers, sizeof(char *));
    assert(pids && sandbox_paths && "tf_run_all_parallel: out of memory");

    int running    = 0;
    int index      = 0;
    int test_count = (int)_tf_tests.count;

    while (index < test_count || running > 0) {
        if (tf_interrupted) {
            log_msg(TF_YELLOW "\nInterrupted, terminating workers..." TF_RESET);
            tf__kill_and_reap(pids, sandbox_paths, max_workers);
            goto done;
        }

        while (running < max_workers && index < test_count && !tf_interrupted) {
            TF_TestCase *t = &_tf_tests.items[index++];

            char safe[128];
            tf__safe_name(t->name, safe, sizeof safe);
            char tmpl[256];
            snprintf(tmpl, sizeof tmpl, "/tmp/tatr_%s_XXXXXX", safe);
            if (!tf__mkdtemp(tmpl, sizeof tmpl)) {
                log_msg(TF_RED "[ ERROR ]" TF_RESET " %s: mkdtemp failed", t->name);
                _tf.tests_failed++;
                _tf.tests_run++;
                continue;
            }

            int slot = -1;
            for (int i = 0; i < max_workers; i++) {
                if (pids[i] == 0) { slot = i; break; }
            }
            assert(slot >= 0 && "no free slot - running count is wrong");

            sandbox_paths[slot] = strdup(tmpl);
            assert(sandbox_paths[slot] && "strdup failed");

            pid_t pid = fork();
            if (pid == 0) {
                for (int i = 0; i < max_workers; i++)
                    free(sandbox_paths[i]);
                free(pids);
                free(sandbox_paths);
                tf__run_child(t, tmpl);
                abort(); // unreachable

            } else if (pid > 0) {
                pids[slot] = pid;
                running++;

            } else {
                perror("fork");
                fs_delete_recursive(tmpl);
                free(sandbox_paths[slot]);
                sandbox_paths[slot] = NULL;
                _tf.tests_failed++;
                _tf.tests_run++;
            }
        }

        if (running > 0) {
            int status;
            pid_t done = waitpid(-1, &status, 0);
            if (done <= 0) continue;

            running--;
            _tf.tests_run++;

            int slot = -1;
            for (int i = 0; i < max_workers; i++) {
                if (pids[i] == done) { slot = i; break; }
            }
            if (slot < 0) continue;

            bool passed = WIFEXITED(status) && WEXITSTATUS(status) == 0;
            if (passed) _tf.tests_passed++;
            else        _tf.tests_failed++;

            if ((passed || delete_fail_tmp) && fs_file_exists(sandbox_paths[slot]))
                fs_delete_recursive(sandbox_paths[slot]);

            free(sandbox_paths[slot]);
            sandbox_paths[slot] = NULL;
            pids[slot]          = 0;
        }
    }

done:
    free(pids);
    free(sandbox_paths);
}

#endif // _WIN32

// ---------------------------------------------------------------------------
// Cleanup + report
// ---------------------------------------------------------------------------

static inline void tf_cleanup(void)
{
    da_free(_tf_tests);
    _tf_tests.items    = NULL;
    _tf_tests.count    = 0;
    _tf_tests.capacity = 0;
}

static inline int tf_report(void)
{
    log_msg("\n" TF_BOLD "----------------------------------------" TF_RESET);
    log_msg(TF_BOLD "%d suites  %d tests" TF_RESET "  --  "
            TF_GREEN "%d passed" TF_RESET "  " TF_RED "%d failed" TF_RESET,
            _tf.suites_run, _tf.tests_run, _tf.tests_passed, _tf.tests_failed);
    log_msg(TF_BOLD "----------------------------------------\n" TF_RESET);
    return _tf.tests_failed > 0 ? 1 : 0;
}

#endif // TATR_TEST_FRAMEWORK_H_
