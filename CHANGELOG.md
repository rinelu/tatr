# Version Conventions

We follow https://semver.org/:

**MAJOR.MINOR.PATCH**

- **PATCH**: bug fixes, no behavior changes  
- **MINOR**: backward-compatible improvements and features  
- **MAJOR**: breaking CLI or storage format changes  

### Notes

- Changes to issue file format are considered **MAJOR**
- Command behavior should remain backward-compatible within MINOR

## 1.3.0 (2026-04-07)

- Add new logging system (`log.h`) with levels, colors, and optional file output
- Add `log_confirm()` for prompts
- Add test suite (framework + command coverage)
- Add test flags (`--out-to-file`, `--jobs`)
- Add `fs_mkdir_force()` with force support
- Fix recursive delete path handling and memory safety
- Fix `close` command result handling
- Fix attach null-termination bug
- Fix attachls, search exit codes
- Fix show output + attachment count format
- Enhance issue ID generation (timestamp + random)
- Replace all `ui_*` calls with `log_*` across commands
- Simplify UI layer (delegate output to logger)
- Improve `log.h` (thread-safety, colors, file output, API cleanup)
- Improve status formatting and colors
- Clean up global help output
- Rewrite GitHub Actions to use CMake
- Add cross-platform secure RNG (`arc4random_buf`, `getrandom`, `BCryptGenRandom`)
- Refactor issue ID generation
- Improve portability and compiler handling
- Minor cleanup and fixes

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
