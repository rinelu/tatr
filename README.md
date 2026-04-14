# tatr

[![CI](https://github.com/rinelu/tatr/actions/workflows/tatr.yaml/badge.svg)](https://github.com/rinelu/tatr/actions)

**tatr** is a small, file-based issue tracker.  

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
search      Search issues
export      Export (markdown/json)
```

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
    -DTATR_ENABLE_UBSAN=ON
```
### Install
```sh
cmake --install build
```
</details>
