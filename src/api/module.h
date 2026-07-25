#ifndef TATR_MODULE_H_
#define TATR_MODULE_H_

#include "error.h"
#include <stddef.h>

typedef enum {
    // This module registers at least one commands/ entry point.
    TATR_MODCAP_CLI_COMMAND = 1u << 0,

    // This module's functionality is reachable with no CLI/argv at all --
    TATR_MODCAP_HEADLESS    = 1u << 1,
} Tatr_Module_Capabilities;

typedef struct {
    const char *name;
    const char *description;
    // Called once at process start if the module is compiled in.
    // May be NULL if the module needs no initialization.
    Tatr_Error (*init)(void);
    unsigned capabilities;
} Tatr_Module;

// The compile-time module table
extern const Tatr_Module *const *tatr_modules;
extern const size_t              tatr_module_count;

// Look up a compiled-in module by name.
// NULL if not found (including if it exists but was compiled out).
const Tatr_Module *tatr_module_find(const char *name);

// Call every compiled-in modules init(), in table order.
// Stops and returns the first non-TATR_OK error.
Tatr_Error tatr_modules_init(void);

#endif // TATR_MODULE_H_
