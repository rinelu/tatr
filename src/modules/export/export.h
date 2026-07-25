#ifndef EXPORT_H_
#define EXPORT_H_

#include <stdio.h>
#include <stdbool.h>
#include "issue.h"
#include "fs.h"
#include "module.h"

typedef struct {
    bool pretty;
    bool embed;
    bool compress;
} Export_Opts;

typedef struct {
    const char *name;
    void (*render)(const Issue *iss, FILE *out, const Export_Opts *opts);
} Exporter;

Exporter *export_find(const char *name);
Exporter *export_get(size_t index);

size_t export_count(void);
void   export_list_all(void);

// Built-in exporters (implemented elsewhere)
void export_markdown(const Issue *iss, FILE *out, const Export_Opts *opts);
void export_json(const Issue *iss, FILE *out, const Export_Opts *opts);

// The formal Tatr_Module instance for this module (design doc §8),
// registered into tatr_modules[] by modules/registry.c.
extern const Tatr_Module TATR_MODULE_EXPORT_DEF;

#endif // EXPORT_H_
