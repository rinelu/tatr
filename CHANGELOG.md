# Version Conventions

We follow https://semver.org/:

**MAJOR.MINOR.PATCH**

- **PATCH**: bug fixes, no behavior changes  
- **MINOR**: backward-compatible improvements and features  
- **MAJOR**: breaking CLI or storage format changes  

### Notes

- Changes to issue file format are considered **MAJOR**
- Command behavior should remain backward-compatible within MINOR

# Changelogs

## 1.5.0 (2026-04-16)

- Add proper checkpoint system using `Temp_Checkpoint` (block + offset)
- Add `temp_init()` and `temp_destroy()` lifecycle functions
- Add `temp_alloc_aligned()` for explicit alignment control
- Add alignment handling (`align_up`)
- Implement linked-block arena with automatic growth
- Replace fixed-size temporary buffer with dynamic arena allocator
- Remove `TEMP_CAPACITY` limitation (no more hard cap / assert on large workloads)
- Make `temp_alloc()` alignment-safe by default
- Update all `tmark` to use `Temp_Checkpoint` instead of `size_t`
- Ensure safe rewind behavior across multiple allocated blocks
- Prevent undefined behavior from checkpoint packing

## 1.4.0 (2026-04-14)

- Add automatic exporter source listing
- Add modular export system with pluggable `Exporter` interface
- Add `export.h` and split exporters into separate modules (`markdown.c`, `json.c`)
- Add dynamic exporter registry and lookup (`export_find`, `export_get`)
- Add `--format / -f` flag to select export format
- Add `--list-format / -L` to list supported export formats
- Add `--pretty / -p` and `--minify / -m` options for JSON output

- Implement full JSON exporter:
- Move Markdown renderer into dedicated module (no behavior change)

- Improve `clag` API:
  - Split `clag__choices` into `clagc__choices` (context) and wrapper
  - Fix linker error with `clag_global_context` across translation units

- Refactor export command:
  - Remove monolithic implementation
  - Separate CLI handling from rendering logic

## 1.3.1 (2026-04-13)

- Add `clag_reset()` call before command execution to avoid state leakage between commands
- Add cross-platform `chdir` wrapper (`tf__chdir`)
- Add new `export` command (Markdown/JSON support)
- Add require_repo() guard to all repository-dependent commands
- Add sequential test runner fallback on Windows (no fork support)
- Add sorting for attachment listing (`attachls`) output
- Add Win32 sandbox creation (`tf__mkdtemp`) and path handling
- Add Windows support for test framework and helpers
- Clean up test helper formatting and argument alignment
- Ensure temporary file cleanup after successful save

- Fix `issue_save()` to safely write via temp file and avoid data loss on failure
- Fix `sv_has` API to use `char` delimiter instead of `const char *`
- Fix missing `da_free` call on failure in `status` command
- Fix path sanitization to handle Windows separators (`\`)
- Fix potential deadlocks in Windows pipe reading (overlapped I/O)
- Fix potential memory issue by ensuring null-terminated temp paths

- Implement cross-platform process execution (`tatr`) with Windows pipe handling
- Improve child process management and cleanup logic
- Improve error handling and cleanup paths in multiple commands
- Improve pipe draining logic for both POSIX and Windows implementations
- Improve sandbox lifecycle handling across platforms
- Improve search message when no results are found
- Improve signal handling portability (`signal` vs `sigaction`)

- Minor consistency and readability improvements
- Minor consistency and readability improvements across test utilities
- Move `cmp_paths` helper to shared `util.h`

- Refactor command table and helper visibility using `NEED_CMD_HELPER`
- Refactor parallel runner internals (naming, cleanup, robustness)
- Refactor test runner to use shared `tf__run_one` logic

- Remove duplicate `cmp_paths` implementations across commands
- Remove duplicate includes and clean up headers
- Replace direct POSIX includes with platform-specific conditionals
- Update all call sites to match new `sv_has` signature

## 1.3.0 (2026-04-07)

- Rewrite GitHub Actions to use CMake
- Improve portability and compiler handling
- Simplify test CMake config (remove redundant flags)
- Ignore build-debug/ directory
- Minor code cleanup and consistency improvements

- Add new logging system (`log.h`) with levels, colors, and optional file output
- Improve `log.h` (thread-safety, colors, file output, API cleanup)
- Replace all `ui_*` calls with `log_*` across commands
- Simplify UI layer (delegate output to logger)
- Add `log_confirm()` for prompts

- Add test suite (framework + command coverage)
- Add test flags (`--out-to-file`, `--jobs`)
- Add proper test cleanup (`tf_cleanup`)
- Fix test framework cleanup (prevent leaks across runs)

- Add cross-platform secure RNG (`arc4random_buf`, `getrandom`, `BCryptGenRandom`)
- Enhance issue ID generation (timestamp + random)
- Refactor issue ID generation

- Add `fs_mkdir_force()` with force support

- Fix memory leak in new command (free issue ID buffer)
- Fix potential memory leaks in attach command
- Fix recursive delete path handling and memory safety
- Fix close command result handling
- Fix attach null-termination bug
- Fix attachment rename detection logic
- Fix attachls, search exit codes
- Fix show output + attachment count format
- Fix use-after-modification in `edit` command (preserve old field value before update)
- Ensure proper memory handling when logging previous values

- Improve status formatting and colors
- Clean up global help output

## 1.2.0 (2026-04-04)

- Improve fs layer cross-platform support (Windows compatibility)
- Fix file size detection on Windows (`_ftelli64` usage)
- Fix recursive delete logic (avoid double deletion)
- Fix file descriptor handling (`close(-1)` guard)
- Fix file reading logic (`fread` usage and buffer sizing)
- Add missing Windows includes and API usage fixes
- Minor internal cleanup and bug fixes

## 1.1.0 (2026-04-04)

- Make clag.h windows compatible

## 1.0.0 (2026-04-04)

- Initial release of tatr

### Features

- Core commands:
  - `init`, `status`
  - `new`, `list`, `show`, `edit`
  - `comment`, `search`
  - `tag`, `close`, `reopen`
  - `delete`
  - `attach`, `attachls`, `detach`
- Global help + per-command help support
- CLI parsing with clag

### Issue System

- Header fields:
  - `title`, `status`, `priority`, `tags`, `created`
- Body separated using `---`
- In-place field editing
- Tag management (add/remove)
- Full-text search across issues

### Attachments

- Per-issue attachment directory
- Add, list, and remove attachments
