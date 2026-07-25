#include "export.h"
#include "array.h"
#include "log.h"

#include <string.h>

static Exporter exporters[] = {
    { "markdown", export_markdown },
    { "json",     export_json     },
};
#define EXPORT_COUNT (int)ARRAY_LEN(exporters)

Exporter *export_find(const char *name)
{
    if (!name) return NULL;
    for (size_t i = 0; i < ARRAY_LEN(exporters); i++) {
        if (strcmp(exporters[i].name, name) != 0) continue;
        return &exporters[i];
    }
    return NULL;
}

Exporter *export_get(size_t index)
{
    if (index >= EXPORT_COUNT) return NULL;
    return &exporters[index];
}

size_t export_count(void)
{
    return EXPORT_COUNT;
}

void export_list_all(void)
{
    log_msg("Currently supported formats:");
    for (int i = 0; i < EXPORT_COUNT; i++)
        log_msg("  %s", exporters[i].name);
}

static Tatr_Error export_module_init(void)
{
    for (size_t i = 0; i < ARRAY_LEN(exporters); i++)
        if (!exporters[i].name || !exporters[i].render)
            return TATR_ERR_UNKNOWN;
    return TATR_OK;
}

const Tatr_Module TATR_MODULE_EXPORT_DEF = {
    .name         = "export",
    .description  = "Render an issue to Markdown or JSON",
    .init         = export_module_init,
    .capabilities = TATR_MODCAP_CLI_COMMAND | TATR_MODCAP_HEADLESS,
};
