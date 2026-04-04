# tatr - tiny issues tracker

tatr is a minimal, file-based issue tracker. It stores issues as plain text files in a .tatr directory.

## Issue Format

Each issue is stored as a plain text file with metadata at the top and body content below a separator line.

Example:
```txt
title: Some bug
status: open
priority: high
tags: bug,somethings
created: 2026-01-01

---
There is a bug somewhere.
```

## Workflow Example

```sh
# Initialize repo
tatr init

# Create issue
tatr new "Some bug"

# List issues
tatr list

# View issue
tatr show <id>

# Add tag
tatr tag <id> bug

# Close issue
tatr close <id>
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
make BUILD=Debug      # default
make BUILD=Release
```

### Optional features

```sh
make WARNINGS=1       # enable strict warnings (default)
make ASAN=1           # enable AddressSanitizer
make UBSAN=1          # enable UndefinedBehaviorSanitizer
```

You can combine options:

```sh
make BUILD=Debug ASAN=1 UBSAN=1
```

### Other targets

```sh
make clean            # remove build artifacts
make install          # install to /usr/local/bin (or PREFIX)
make PREFIX=/usr/bin install
make info             # show current configuration
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
