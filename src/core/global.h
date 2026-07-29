#ifndef GLOBAL_H_
#define GLOBAL_H_

#include <limits.h>

#ifndef PATH_MAX
#ifdef _WIN32
#include <stdlib.h> // for _MAX_PATH
#define PATH_MAX _MAX_PATH
#else
#define PATH_MAX 4096
#endif
#endif // PATH_MAX

extern char g_repo_root[PATH_MAX]; // Absolute path

#define TATR_VERSION "3.0.0 (2026-07-25)"
#define TATRLOG_SEPARATOR "--- entry ---"

#ifdef _WIN32
#define USERNAME_ENV getenv("USERNAME")
#else
#define USERNAME_ENV getenv("USER")
#endif

// Build a path rooted at the repo.
// Returns a temp-allocated string: "<repo_root>/<rel>".
// If repo_root is empty (not yet found), returns rel unchanged.
const char *tatr_path(const char *rel);

#define TATR_DIR_PATH       tatr_path(".tatr")
#define TATR_ISSUES_PATH    tatr_path(".tatr" FS_SEP "issues")
#define TATR_CONFIG_PATH    tatr_path(".tatr" FS_SEP "config")
#define TATR_LOG_PATH       tatr_path(".tatr" FS_SEP "log")
#define TATR_VERSION_PATH   tatr_path(".tatr" FS_SEP "VERSION")

#endif // GLOBAL_H_
