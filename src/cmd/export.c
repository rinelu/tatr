#include "cmd.h"

static void render_markdown(const Issue *iss, FILE *out)
{
    // Title
    fprintf(out, "# "SV_Fmt"\n\n", SV_Arg(iss->title));

    // Metadata
    fprintf(out, "| Field    | Value |\n");
    fprintf(out, "|----------|-------|\n");
    fprintf(out, "| **ID**       | `"SV_Fmt"` |\n", SV_Arg(iss->id));
    fprintf(out, "| **Status**   | "SV_Fmt" |\n",   SV_Arg(iss->status));
    fprintf(out, "| **Priority** | "SV_Fmt" |\n",   SV_Arg(iss->priority));
    fprintf(out, "| **Tags**     | "SV_Fmt" |\n",   SV_Arg(iss->tags));
    fprintf(out, "| **Created**  | "SV_Fmt" |\n",   SV_Arg(iss->created));

    // Body
    if (iss->body.count > 0) {
        fprintf(out, "\n## Description\n\n");

        String_View cursor = iss->body;
        bool in_comment = false;
        int  comment_n  = 0;

        while (cursor.count > 0) {
            String_View line = sv_slice_by_delim(&cursor, '\n');
            String_View trimmed = sv_trim(line);

            if (sv_eq_cstr(trimmed, "---comment---")) {
                if (!in_comment) {
                    fprintf(out, "\n---\n");
                    fprintf(out, "\n## Comments\n");
                }
                in_comment = true;
                comment_n++;
                fprintf(out, "\n### Comment %d\n\n", comment_n);
                continue;
            }

            // Inside a comment block: "date:" and "author:" become bold pairs
            if (in_comment) {
                String_View rest = line;
                String_View key  = sv_trim(sv_slice_by_delim(&rest, ':'));

                if (sv_eq_cstr(key, "date") || sv_eq_cstr(key, "author")) {
                    fprintf(out, "**"SV_Fmt":** "SV_Fmt"\n",
                            SV_Arg(key), SV_Arg(sv_trim(rest)));
                    continue;
                }

                // Blank line between meta and text
                if (trimmed.count == 0) {
                    fprintf(out, "\n");
                    continue;
                }
            }

            fprintf(out, SV_Fmt"\n", SV_Arg(line));
        }
    }

    // TODO: embed base64 content for images when --embed flag is added
    File_Paths files = {0};
    if (fs_read_dir(iss->attach_path, &files) && files.count > 0) {
        fprintf(out, "\n---\n\n## Attachments\n\n");
        for (size_t i = 0; i < files.count; i++)
            fprintf(out, "- `%s`\n", files.items[i]);
        da_free(files);
    }
}

static void render_json(const Issue *iss, FILE *out)
{
    /* TODO: implement JSON renderer.
     *
     * Planned structure:
     * {
     *   "id":       "<id>",
     *   "title":    "<title>",
     *   "status":   "<status>",
     *   "priority": "<priority>",
     *   "tags":     ["<tag>", ...],
     *   "created":  "<iso>",
     *   "body":     "<body text>",
     *   "comments": [
     *     { "date": "...", "author": "...", "body": "..." },
     *     ...
     *   ],
     *   "attachments": ["<filename>", ...]
     * }
     *
     * Needs a small JSON string-escaper for body/title content.
     */
    (void)iss;
    (void)out;
    log_error("JSON export is not yet implemented");
}

int cmd_export(int argc, char **argv)
{
    bool  *fmt_markdown = clag_bool("markdown", 'M', true,  "Export as Markdown");
    bool  *fmt_json     = clag_bool("json",     'J', false, "Export as JSON (not yet implemented)");
    char **output       = clag_str ("output",   'o', "stdout",  "Write to file instead of stdout");

    clag_usage("<id> [options]");
    clag_mutex("json", "markdown");

    if (!clag_parse(argc, argv)) {
        clag_print_error(stderr);
        clag_print_options(stderr);
        return 1;
    }

    if (clag_rest_argc() < 1) {
        log_error("missing issue ID");
        log_msg("usage: tatr export <id> [--markdown] [--json] [--output <file>]");
        return 1;
    }
 
    const char *id = clag_rest_argv()[0];

    size_t tmark = temp_save();
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

    if (*fmt_json) {
        log_error("JSON export is not yet implemented (use --markdown)");
        goto defer;
    }

    if (*fmt_markdown) render_markdown(&iss, out);

    if (out != stdout) {
        fclose(out);
        log_info("Exported issue %s -> %s", id, *output);
    }

    result = 0;

defer:
    issue_free(&iss);
    temp_rewind(tmark);
    return result;
}
