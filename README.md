# tatr

[![CI](https://github.com/rinelu/tatr/actions/workflows/tatr.yaml/badge.svg)](https://github.com/rinelu/tatr/actions)
[![License: MIT / Unlicense](https://img.shields.io/badge/license-MIT%20%2F%20Unlicense-blue.svg)](LICENSE)

**tatr** is a small, fast, file-based issue tracker that lives in your repo.
Issues are plain text files that diff, grep, and merge like any other file you already version with git.

> **Demo:** _coming soon_ - a short terminal recording will go here.

## Why tatr

- Every issue is a human-readable file. Read it with `cat`, grep across your backlog, review it in a pull request diff.
- Works entirely offline, scoped to whatever git repo (or directory) you run it in.
- Issues live next to your code, travel with `git clone`, and merge with the same tooling you already use.

## Quick start

```sh
tatr init
tatr new "Fix crash on startup"
tatr list
tatr show <id>
```

Basic workflow:

```sh
tatr tag <id> bug
tatr close <id>
tatr reopen <id>
```

## Issue format

Each issue is stored as a plain text file:

```txt
title: Fix crash on startup
status: open
priority: high
tags: bug,urgent
created: 2026-01-01

---
Stack trace shows null pointer dereference.
```

## Commands

```
init        Initialize a repository
new         Create a new issue
list        List issues
show        Show issue details
edit        Edit fields
tag         Add/remove tags
close       Close an issue
reopen      Reopen an issue
delete      Delete an issue
attach      Attach files
attachls    List attachments
detach      Remove attachment
comment     Add a comment to an issue
search      Search issues
status      Show aggregate counts (open/closed, by priority, by tag)
log         Show the event log
config      Get/set configuration values
export      Export an issue (markdown/json)
```

Run `tatr <command> --help` for flags on any individual command.

## Configuration

```sh
tatr config --keys     # list all valid keys
tatr config author "some1"
tatr config default_priority high
```

| Key | Description | Default |
|---|---|---|
| `author` | Default author name for comments and log entries | `unknown` |
| `default_status` | Default status for new issues | `open` |
| `default_priority` | Default priority for new issues | `normal` |
| `default_editor` | Editor to use instead of `$VISUAL`/`$EDITOR` | _(unset)_ |
| `log.limit` | Default `--limit` for `tatr log` | `0` (no limit) |
| `list.show_closed` | Show closed issues in `tatr list` by default | `false` |
| `list.limit` | Default `--limit` for `tatr list` | `0` (no limit) |

Config is layered: a local, repo-scoped config overrides a global, user-scoped one.

## Build

`tatr` supports both **Makefile** and **CMake** builds.

<details open>
<summary><strong>Makefile</strong></summary>

### Basic usage
```sh
make
```
### Build configurations
```sh
make BUILD=Debug   # default
make BUILD=Release
```
### Optional features
```sh
make WARNINGS=1 # enable strict warnings (default)
make ASAN=1     # enable AddressSanitizer
make UBSAN=1    # enable UndefinedBehaviorSanitizer
```
You can combine options:
```sh
make BUILD=Debug ASAN=1 UBSAN=1
```
### Other targets
```sh
make clean   # remove build artifacts
make install # install to /usr/local/bin (or PREFIX)
make PREFIX=/usr/bin install
make info # show current configuration
```
</details>

<details>
<summary><strong>CMake</strong></summary>

### Configure
```sh
cmake -S . -B build
```
### Build
```sh
cmake --build build
```
### Build types
```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
```
### Options
```sh
cmake -S . -B build \
    -DTATR_ENABLE_WARNINGS=ON \
    -DTATR_ENABLE_ASAN=ON \
    -DTATR_ENABLE_UBSAN=ON \
    -DTATR_MODULE_EXPORT=ON
```
### Install
```sh
cmake --install build
```
</details>

Tests run via `ctest` or `make -C tests`.

## Contributing

Issues and PRs are welcome. See [`src/modules/README.md`](src/modules/README.md) if
you're adding a new optional feature modules.

## License

See [`LICENSE`](LICENSE) for full text.
