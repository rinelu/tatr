#include "cmd.h"
#include "temp.h"
#include "export.h"
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

void export_list_all(void)
{
    log_msg("Currently supported formats:");
    for (int i = 0; i < EXPORT_COUNT; i++)
        log_msg("  %s", exporters[i].name);
}

int cmd_export(int argc, char **argv)
{
    char **format       = clag_str("format",       'f',        "markdown", "Export format");
    char **output       = clag_str ("output",      'o', NULL,  "Write to file instead of stdout");
    bool  *ls_format    = clag_bool("list-format", 'L', NULL,  "List all supported format.");
    bool  *pretty       = clag_bool("pretty",      'p', true,  "Pretty JSON output");
    bool  *minify       = clag_bool("minify",      'm', false, "Minified JSON output");

    clag_choices("format", "markdown", "json");
    clag_usage("<id> [options]");
    clag_mutex("pretty", "minify");

    if (!clag_parse(argc, argv)) {
        clag_print_error(stderr);
        return 1;
    }

    if (*ls_format) {
        export_list_all();
        return 0;
    }

    if (clag_rest_argc() < 1) {
        log_error("missing issue ID");
        log_msg("usage: tatr export <id> [--format=] [--output <file>]");
        return 1;
    }

    if (!require_repo()) return 1;

    const char *id = clag_rest_argv()[0];

    Temp_Checkpoint tmark = temp_save();
    int    result = 1;
    Issue  iss;

    if (!issue_load(id, &iss)) {
        log_error("issue '%s' not found", id);
        goto defer;
    }

    // Open output destination
    FILE *out = stdout;
    if (*output && **output) {
        out = fopen(*output, "w");
        if (!out) {
            log_error("cannot open output file '%s'", *output);
            goto defer;
        }
    }

    Exporter *exp = export_find(*format);

    if (!exp) {
        log_error("unknown format '%s'", *format);

        log_hint("available formats:");
        for (size_t i = 0; i < ARRAY_LEN(exporters); i++) {
            log_msg("  %s", export_get(i)->name);
        }

        return 1;
    }

    Export_Opts opts = {
        .pretty = *pretty && !*minify,
        .embed  = false,
    };

    exp->render(&iss, out, &opts);
    result = 0;

defer:
    issue_free(&iss);
    temp_rewind(tmark);
    return result;
}

