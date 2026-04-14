#include "export.h"

static void json_escape(FILE *out, String_View sv)
{
    for (size_t i = 0; i < sv.count; i++) {
        char c = sv.data[i];
        switch (c) {
            case '\"': fputs("\\\"", out); break;
            case '\\': fputs("\\\\", out); break;
            case '\n': fputs("\\n",  out); break;
            case '\r': fputs("\\r",  out); break;
            case '\t': fputs("\\t",  out); break;
            default:
                if ((unsigned char)c < 32) fprintf(out, "\\u%04x", c);
                else fputc(c, out);
        }
    }
}

static void json_write_tags(FILE *out, String_View tags)
{
    fprintf(out, "[");

    bool first = true;
    while (tags.count > 0) {
        String_View t = sv_slice_by_delim(&tags, ',');
        t = sv_trim(t);

        if (t.count == 0) continue;

        if (!first) fprintf(out, ", ");
        fprintf(out, "\"");
        json_escape(out, t);
        fprintf(out, "\"");

        first = false;
    }

    fprintf(out, "]");
}

static void json_write_comments(FILE *out, String_View body)
{
    fprintf(out, "[");

    String_View cursor = body;
    bool in_comment = false;
    bool first_comment = true;

    String_View date = {0};
    String_View author = {0};

    String_Builder text = {0};

    while (cursor.count > 0) {
        String_View line = sv_slice_by_delim(&cursor, '\n');
        String_View trimmed = sv_trim(line);

        if (sv_eq_cstr(trimmed, "---comment---")) {
            // flush previous comment
            if (in_comment) {
                if (!first_comment) fprintf(out, ", ");

                fprintf(out, "{ ");
                fprintf(out, "\"date\": \"");   json_escape(out, date);   fprintf(out, "\", ");
                fprintf(out, "\"author\": \""); json_escape(out, author); fprintf(out, "\", ");
                fprintf(out, "\"body\": \"");   json_escape(out, sb_to_sv(text)); fprintf(out, "\"");
                fprintf(out, " }");

                first_comment = false;
                sb_free(text);
                text = (String_Builder){0};
                date = (String_View){0};
                author = (String_View){0};
            }

            in_comment = true;
            continue;
        }

        if (!in_comment) continue;

        String_View rest = line;
        String_View key  = sv_trim(sv_slice_by_delim(&rest, ':'));

        if (sv_eq_cstr(key, "date")) {
            date = sv_trim(rest);
            continue;
        }

        if (sv_eq_cstr(key, "author")) {
            author = sv_trim(rest);
            continue;
        }

        // body text
        if (trimmed.count == 0) {
            sb_append_cstr(&text, "\n");
        } else {
            sb_append_buf(&text, line.data, line.count);
            sb_append_cstr(&text, "\n");
        }
    }

    // flush last comment
    if (in_comment) {
        if (!first_comment) fprintf(out, ", ");

        fprintf(out, "{ ");
        fprintf(out, "\"date\": \"");   json_escape(out, date);   fprintf(out, "\", ");
        fprintf(out, "\"author\": \""); json_escape(out, author); fprintf(out, "\", ");
        fprintf(out, "\"body\": \"");   json_escape(out, sb_to_sv(text)); fprintf(out, "\"");
        fprintf(out, " }");

        sb_free(text);
    }

    fprintf(out, "]");
}

static void json_write_attachments(FILE *out, const char *path)
{
    fprintf(out, "[");

    File_Paths files = {0};
    bool first = true;

    if (fs_read_dir(path, &files) && files.count > 0) {
        for (size_t i = 0; i < files.count; i++) {
            if (!first) fprintf(out, ", ");
            fprintf(out, "\"%s\"", files.items[i]);
            first = false;
        }
    }

    if (files.items) da_free(files);

    fprintf(out, "]");
}

typedef struct {
    bool pretty;
    int  indent;
} Json_Fmt;

static void json_indent(FILE *out, Json_Fmt fmt)
{
    if (!fmt.pretty) return;
    for (int i = 0; i < fmt.indent; i++) fputs("  ", out);
}

static void json_nl(FILE *out, Json_Fmt fmt)
{
    if (fmt.pretty) fputc('\n', out);
}

static void json_key(FILE *out, Json_Fmt fmt, const char *key)
{
    json_indent(out, fmt);
    fprintf(out, "\"%s\":", key);
    if (fmt.pretty) fputc(' ', out);
}

void export_json(const Issue *iss, FILE *out, const Export_Opts *opts)
{
    Json_Fmt fmt = {
        .pretty = opts->pretty,
        .indent = 0,
    };

    fprintf(out, "{"); json_nl(out, fmt);
    fmt.indent++;

    json_key(out, fmt, "id");
    fprintf(out, "\""); json_escape(out, iss->id); fprintf(out, "\","); json_nl(out, fmt);

    json_key(out, fmt, "title");
    fprintf(out, "\""); json_escape(out, iss->title); fprintf(out, "\","); json_nl(out, fmt);

    json_key(out, fmt, "status");
    fprintf(out, "\""); json_escape(out, iss->status); fprintf(out, "\","); json_nl(out, fmt);

    json_key(out, fmt, "priority");
    fprintf(out, "\""); json_escape(out, iss->priority); fprintf(out, "\","); json_nl(out, fmt);

    json_key(out, fmt, "tags");
    json_write_tags(out, iss->tags); fprintf(out, ","); json_nl(out, fmt);

    json_key(out, fmt, "created");
    fprintf(out, "\""); json_escape(out, iss->created); fprintf(out, "\","); json_nl(out, fmt);

    json_key(out, fmt, "body");
    fprintf(out, "\""); json_escape(out, iss->body); fprintf(out, "\","); json_nl(out, fmt);

    json_key(out, fmt, "comments");
    json_write_comments(out, iss->body); fprintf(out, ","); json_nl(out, fmt);

    json_key(out, fmt, "attachments");
    json_write_attachments(out, iss->attach_path); json_nl(out, fmt);

    fmt.indent--;
    json_indent(out, fmt);
    fprintf(out, "}"); json_nl(out, fmt);
}


