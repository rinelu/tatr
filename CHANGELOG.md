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

## 2.0.0 (2026-04-18) (ON-GOING)

- New `.tatr/log` file introduced for history tracking
- Add persistent event log system (`tatrlog`)
- Add new `log` command to inspect history (filter by id, event, date, limit)
- Track all mutating actions: create, edit, close, reopen, delete, tag, comment, attach, detach
- Add structured and colorized log output with relative and full timestamps
- Add reusable timestamp formatting utilities
- Add logging hooks for all operations
- Standardize ISO and relative time formatting across commands
- Replace raw day counts in `status` command with human-readable relative time (e.g. "2d ago")
- Simplify and standardize `search` result output formatting

- Add full `$EDITOR` integration:
  - Edit entire issue (`tatr edit <id>`)
  - Edit specific fields via editor (`--field`)
  - Interactive issue creation when no fields provided
  - Optional interactive body editing (`-i`)

- Add configuration system (`config.*`):
  - Support local (`.tatr/config`) and global (`~/.config/tatr/config`) scopes
  - Implement layered config resolution (local overrides global, fallback to defaults)
  - Add typed config key definitions with descriptions and defaults
  - Add helper APIs: `config_get`, `config_get_or_default`, `config_set`, `config_unset`

- Add `config` command:
  - Get/set configuration values (`tatr config <key> [value]`)
  - Add `--local` and `--global` scope selection
  - Add `--list` to display stored and resolved configuration values
  - Add `--unset` to remove keys
  - Add `--edit` to open config file in `$EDITOR`
  - Add `--keys` to list all valid config keys with descriptions and defaults

- Add config file editing support:
  - Auto-create config file with documented template on first edit
  - Include inline comments describing all available keys and defaults
  - Respect `$EDITOR` and configured `default_editor`

- Add default configuration keys:
  - `author`
  - `default_status`
  - `default_priority`
  - `default_editor`
  - `log.limit`
  - `list.show_closed`
  - `list.limit`

- Add cross-platform config path resolution:
  - Support XDG base directory (`$XDG_CONFIG_HOME`)
  - Fallback to `$HOME/.config`
  - Windows support via `%APPDATA%`

- Add MIME detection system (`mime.*`):
  - Detect file types using magic numbers and signatures
  - Fallback to extension-based detection
  - Heuristic detection for text vs binary content
  - Helpers for identifying text and image types

- Add codec utilities (`codec.*`):
  - Implement base64 encoding and decoding
  - Enable embedding binary data in export formats

- Add filesystem utilities:
  - Add `fs_file_extension` helper
  - Improve `fs_unique_path` to ensure proper null-termination and safe path building

- Add new modules:
  - `editor.*` (external editor integration)
  - `tatrlog.*` (logging system)
  - `mime.*` (MIME type detection)
  - `codec.*` (encoding utilities)

- Improve configuration UX:
  - Show source of each value (local/global/default) in resolved view
  - Highlight unknown keys found in config files
  - Provide helpful error messages and hints for invalid keys

- Improve file handling for config:
  - Preserve comments and formatting when modifying config files
  - Ensure safe directory creation for config paths
  - Support idempotent updates and clean key replacement

- Redesign `new` command:
  - Remove required `--title`
  - Support full interactive mode
  - Support hybrid CLI + editor workflows
  - Improve issue initialization flow

- Improve `edit` command:
  - Support full document editing
  - Support editor-based field editing
  - Add direct field updates with change logging
  - Add `body` as editable field

- Improve `delete` and `detach` command:
  - Support deleting multiple issues in a single invocation
  - No prompt by default
  - Add `--interactive` for interactive confirmation per issue
  - Make `--force` ignore missing issues and suppress errors
  - Continue processing remaining IDs on failure
  - Return non-zero if any deletion fails
  - Ensure logs only record successful deletions

- Improve `attach` command:
  - Detect filename conflicts and auto-rename safely
  - Improve rename detection using resolved destination path
  - Log actual stored attachment path instead of original filename
  - Improve user-facing messages for renamed attachments

- Improve `list` command:
  - Redesign output formatting with aligned columns
  - Add dynamic title wrapping based on terminal width
  - Improve readability with consistent spacing and colors

- Improve tag command:
  - Better add/remove handling
  - Log tag changes

- Improve export system:
  - Add new `--embed` option to include attachment content
  - Add new `--compress` option to control attachment encoding behavior
  - Simplify JSON formatting flags (remove `--pretty`, keep `--minify`)
  - Default to pretty JSON output unless `--minify` is used

- Enhance JSON export:
  - Add consistent indentation and newline handling
  - Export comments as structured objects (`date`, `author`, `body`)
  - Export tags as formatted arrays
  - Add attachment embedding with:
    - MIME type detection
    - Automatic encoding selection (`utf-8` or `base64`)
    - Inline binary data support

- Enhance Markdown export:
  - Add optional embedded attachments via `--embed`
  - Render images inline using base64 data URLs
  - Render text attachments as fenced code blocks with language detection
  - Wrap embedded content in collapsible `<details>` blocks

- Refactor UI layer:
  - Simplify UI into reusable formatting utilities
  - Remove redundant UI state and helpers
  - Centralize output formatting logic

- Improve cross-platform support
  - Enforce C11 standard
  - Add Windows filesystem + temp file handling
  - Add Windows-compatible timestamp (`localtime_s`)
  - Clean up platform-specific includes and randomness handling

- Minor improvements:
  - Add `sv_empty`
  - Add `sb_append_char`
  - Improve formatting in `clag` choices output
  - Cleanup redundant option printing
  - Various consistency fixes and refactors

### BREAKING CHANGES

- Issue creation flow changed (interactive by default when no fields provided)
- Editing behavior changed (can open full editor instead of requiring flags)
- Internal issue raw format handling updated (header/body composition)

- Export behavior changed:
  - Attachments may now be embedded directly in output when using `--embed`
  - JSON attachment format changed from filename list to structured objects when embedded
  - JSON formatting defaults to pretty output unless `--minify` is specified

- Detach command interface changed:
  - Now requires `<id> <file>...` instead of single filename
  - `--yes` flag removed and replaced with `--interactive` / `--force`

- Internal issue model updated:
  - `status` and `priority` fields changed from `String_View` to enums
  - All internal comparisons now use enum values instead of string matching
  - Introduced conversion layer between string and enum
  - Invalid values are detected explicitly instead of being treated as raw strings

- UI module API significantly reduced:
  - Removed `ui_print_*` functions and initialization (`ui_init`)
  - Output rendering is now handled directly in commands
  - `log_init()` must be explicitly called in `main`

- Internal structure refactor:
  - UI no longer owns rendering logic for issues or commands
  - Commands are now responsible for their own output formatting

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
