#ifndef EXPORT_H_
#define EXPORT_H_

#include <stdio.h>
#include <stdbool.h>
#include "issue.h"
#include "fs.h"

typedef struct {
    bool pretty;  // JSON, etc.
    bool embed;   // future: embed attachments (base64)
} Export_Opts;

typedef struct {
    const char *name;
    void (*render)(const Issue *iss, FILE *out, const Export_Opts *opts);
} Exporter;

Exporter *export_find(const char *name);
Exporter *export_get(size_t index);

size_t export_count(void);

// Built-in exporters (implemented elsewhere)
void export_markdown(const Issue *iss, FILE *out, const Export_Opts *opts);
void export_json(const Issue *iss, FILE *out, const Export_Opts *opts);

#endif // EXPORT_H_
