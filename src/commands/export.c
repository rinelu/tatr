#include "cmd.h"
#include "export.h"

int cmd_export(int argc, char **argv)
{
    char **format    = clag_str("format",       'f', "markdown", "Export format");
    char **output    = clag_str ("output",      'o', NULL,        "Write to file instead of stdout");
    bool  *ls_format = clag_bool("list-format", 'L', false,       "List all supported format.");
    bool  *minify    = clag_bool("minify",      'm', false,       "Minified JSON output");
    bool  *embed     = clag_bool("embed",       'e', false,       "Embed attachments directly into output");
    bool  *compress  = clag_bool("compress",    'c', false,       "Compress embedded attachments (e.g. base64 for binary)");

    clag_choices("format", "markdown", "json");
    clag_usage("<id> [options]");

    if (!clag_parse(argc, argv)) {
        clag_print_error(stderr);
        return 1;
    }

    if (*ls_format) {
        export_list_all();
        return 0;
    }

    if (!require_repo()) return 1;

    if (clag_rest_argc() < 1) {
        log_error("missing issue ID");
        log_msg("usage: tatr export <id> [--format=] [--output <file>]");
        return 1;
    }

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
        for (size_t i = 0; i < export_count(); i++) {
            log_msg("  %s", export_get(i)->name);
        }

        return 1;
    }

    Export_Opts opts = {
        .pretty   = !*minify,
        .embed    = *embed,
        .compress = *compress,
    };

    exp->render(&iss, out, &opts);
    result = 0;

defer:
    issue_free(&iss);
    temp_rewind(tmark);
    return result;
}
