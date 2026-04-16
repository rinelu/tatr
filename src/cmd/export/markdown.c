#include "export.h"

void export_markdown(const Issue *iss, FILE *out, const Export_Opts *opts)
{
    (void)opts;

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
