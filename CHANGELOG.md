# Version Conventions

We follow https://semver.org/:

**MAJOR.MINOR.PATCH**

- **PATCH**: bug fixes, no behavior changes  
- **MINOR**: backward-compatible improvements and features  
- **MAJOR**: breaking CLI or storage format changes  

### Notes

- Changes to issue file format are considered **MAJOR**
- Command behavior should remain backward-compatible within MINOR

# Changelog

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
