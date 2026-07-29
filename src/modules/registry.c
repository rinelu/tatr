#include "module.h"
#include <string.h>

#ifdef TATR_MODULE_EXPORT
#include "export.h"
#endif

static const Tatr_Module *const MODULES[] = {

#ifdef TATR_MODULE_EXPORT
    &TATR_MODULE_EXPORT_DEF,
#endif

};

const Tatr_Module *const *tatr_modules      = MODULES;
const size_t              tatr_module_count = sizeof(MODULES) / sizeof(MODULES[0]);

const Tatr_Module *tatr_module_find(const char *name)
{
    if (!name) return NULL;
    for (size_t i = 0; i < tatr_module_count; i++)
        if (strcmp(MODULES[i]->name, name) == 0)
            return MODULES[i];
    return NULL;
}

Tatr_Error tatr_modules_init(void)
{
    for (size_t i = 0; i < tatr_module_count; i++) {
        if (!MODULES[i]->init) continue;
        Tatr_Error err = MODULES[i]->init();
        if (err != TATR_OK) return err;
    }
    return TATR_OK;
}
