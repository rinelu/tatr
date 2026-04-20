#include "codec.h"
#include "export.h"
#include "mime.h"

void export_markdown(const Issue *iss, FILE *out, const Export_Opts *opts)
{
    // Title
    fprintf(out, "# "SV_Fmt"\n\n", SV_Arg(iss->title));

    // Metadata
    fprintf(out, "| Field    | Value |\n");
    fprintf(out, "|----------|-------|\n");
    fprintf(out, "| **ID**       | `"SV_Fmt"` |\n", SV_Arg(iss->id));
    fprintf(out, "| **Status**   | %s |\n",         issue_status_to_cstr(iss->status));
    fprintf(out, "| **Priority** | %s |\n",         issue_priority_to_cstr(iss->priority));
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
                    fprintf(out, "**"SV_Fmt":** "SV_Fmt"\n", SV_Arg(key), SV_Arg(sv_trim(rest)));
                    continue;
                }

                // Blank line between meta and text
                if (sv_empty(trimmed)) {
                    fprintf(out, "\n");
                    continue;
                }
            }

            fprintf(out, SV_Fmt"\n", SV_Arg(line));
        }
    }

    File_Paths files = {0};
    if (fs_read_dir(iss->attach_path, &files) && files.count > 0) {
        fprintf(out, "\n---\n\n## Attachments\n\n");
        for (size_t i = 0; i < files.count; i++) {
            const char *file = files.items[i];
            if (!opts->embed) {
                fprintf(out, "- `%s`\n", file);
                continue;
            }
            const char *mime = mime_from_file(fs_path(iss->attach_path, file));
            bool is_image = mime_is_image(mime);

            fprintf(out, "### `%s`\n\n", file);
            const char *ext = fs_file_extension(file);

            String_Builder fcontent = {0};
            fs_read_file(fs_path(iss->attach_path, file), &fcontent);

            fprintf(out, "<details>\n");
            if (is_image) {
                String_Builder sb = {0};
                b64_encode(&sb, (const unsigned char *)fcontent.items, fcontent.count);

                fprintf(out, "<img alt=\"%s\" src=\"data:%s;base64,", file, mime);
                fprintf(out, SV_Fmt, SB_Arg(sb));
                fprintf(out, "\" style=\"max-width:100%%; height:auto;\" loading=\"lazy\" />\n\n");

                sb_free(sb);
            } else {
                fprintf(out, "```%s\n", ext ? ext : "txt");
                fprintf(out, SV_Fmt, SB_Arg(fcontent));
                fprintf(out, "```\n");
            }
            fprintf(out, "</details>\n");
            sb_free(fcontent);
        }
        da_free(files);
    }
}
