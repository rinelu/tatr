# modules/

Optional, self-contained subsystems. Each module is a plain library of
functionality that knows nothing about the CLI (`commands/`) or about
other modules, can be compiled out entirely, and is wired in from exactly one place.

## Layering

```
commands/          <- CLI only: argv parsing (clag), stdout/stderr, exit codes
modules/<name>/    <- feature engines: no clag, no argv, no direct printing of
                      help text. Take/return plain data (Issue, FILE*, strings...)
modules/registry.c <- the only place that knows which modules are compiled init
                      populates tatr_modules[] (src/api/module.h)
core/              <- fundamental types shared by everything
thirdparty/        <- vendored, general-purpose libs
```

A module must be callable from more than just the CLI. `modules/export`,
for example, exposes `export_find()` / `Exporter.render()` with no
dependency on `cmd.h`. `commands/export.c` is a thin wrapper that parses
`argv` and calls into it.

## Adding a new module

1. Create `modules/<name>/` with its own `.c`/`.h` files. Keep it free of
   `clag.h` / `argv` handling
2. Define a `const Tatr_Module TATR_MODULE_<NAME>_DEF` somewhere in
   `modules/<name>/` (see `modules/export/export.c` for the template):
   name, a one-line description, an optional `init()`, and its
   capabilities (`TATR_MODCAP_CLI_COMMAND` if it exposes a `commands/`
   entry point, `TATR_MODCAP_HEADLESS` if it's callable with no CLI at
   all -- most modules should be both).
3. Register it in `modules/registry.c`: one `#include` and one array entry,
   both guarded by `#ifdef TATR_MODULE_<NAME>`
4. In both `Makefile` and `CMakeLists.txt`: add `MODULE_<NAME>`/`TATR_MODULE_<NAME>`
   build options, glob/list its sources guarded by that option, and add a
   `TATR_MODULE_<NAME>` compile definition so code (including
   `modules/registry.c`) can `#ifdef` it. Use the `TATR_MODULE_EXPORT`
   wiring in both files as the template.
5. If the module needs a CLI entry point, add `commands/<name>.c` with a
   `cmd_<name>()` that parses flags and calls into `modules/<name>/`, and
   guard its declaration + `commands[]` table row in `commands/cmd.h` with
   `#ifdef TATR_MODULE_<NAME>` (see `cmd_export` for the template).
6. Add tests under `tests/` the same way existing features are tested.

## Current modules

- **export** - renders an `Issue` to Markdown or JSON (`tatr export`).
