#include "astring.h"
#include "codec.h"
#include "export.h"
#include "issue.h"
#include "mime.h"

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
                if ((unsigned char)c < 32)
                    fprintf(out, "\\u%04x", c);
                else
                    fputc(c, out);
        }
    }
}

static void json_write_tags(FILE *out, String_View tags, Json_Fmt fmt)
{
    fprintf(out, "[");
    json_nl(out, fmt);

    Json_Fmt inner = fmt;
    inner.indent++;

    bool first = true;

    while (tags.count > 0) {
        String_View t = sv_slice_by_delim(&tags, ',');
        t = sv_trim(t);
        if (sv_empty(t)) continue;

        if (!first) {
            fprintf(out, ",");
            json_nl(out, fmt);
        }

        json_indent(out, inner);
        fprintf(out, "\"");
        json_escape(out, t);
        fprintf(out, "\"");

        first = false;
    }

    json_nl(out, fmt);
    json_indent(out, fmt);
    fprintf(out, "]");
}

static void json_write_comments(FILE *out, String_View body, Json_Fmt fmt)
{
    fprintf(out, "[");
    json_nl(out, fmt);

    Json_Fmt inner = fmt;
    inner.indent++;

    String_View cursor = body;
    bool in_comment = false;
    bool first = true;

    String_View date = {0};
    String_View author = {0};
    String_Builder text = {0};

    while (cursor.count > 0) {
        String_View line = sv_slice_by_delim(&cursor, '\n');
        String_View trimmed = sv_trim(line);

        if (sv_eq_cstr(trimmed, "---comment---")) {
            if (in_comment) {
                if (!first) {
                    fprintf(out, ",");
                    json_nl(out, fmt);
                }

                json_indent(out, inner);
                fprintf(out, "{");
                json_nl(out, fmt);

                Json_Fmt obj = inner;
                obj.indent++;

                json_key(out, obj, "date");
                fprintf(out, "\""); json_escape(out, date); fprintf(out, "\",");
                json_nl(out, obj);

                json_key(out, obj, "author");
                fprintf(out, "\""); json_escape(out, author); fprintf(out, "\",");
                json_nl(out, obj);

                json_key(out, obj, "body");
                fprintf(out, "\""); json_escape(out, sb_to_sv(text)); fprintf(out, "\"");
                json_nl(out, fmt);

                json_indent(out, inner);
                fprintf(out, "}");

                date.data   = NULL; date.count   = 0;
                author.data = NULL; author.count = 0;
                sb_free(text);
                text.items = NULL; text.count = 0; text.capacity = 0;

                first = false;
            }

            in_comment = true;
            continue;
        }

        if (!in_comment) continue;

        String_View rest = line;
        String_View key = sv_trim(sv_slice_by_delim(&rest, ':'));

        if (sv_eq_cstr(key, "date")) {
            date = sv_trim(rest);
        } else if (sv_eq_cstr(key, "author")) {
            author = sv_trim(rest);
        } else {
            sb_append_buf(&text, line.data, line.count);
            sb_append_cstr(&text, "\n");
        }
    }

    if (in_comment) {
        if (!first) {
            fprintf(out, ",");
            json_nl(out, fmt);
        }

        json_indent(out, inner);
        fprintf(out, "{");
        json_nl(out, fmt);

        Json_Fmt obj = inner;
        obj.indent++;

        json_key(out, obj, "date");
        fprintf(out, "\""); json_escape(out, date); fprintf(out, "\",");
        json_nl(out, obj);

        json_key(out, obj, "author");
        fprintf(out, "\""); json_escape(out, author); fprintf(out, "\",");
        json_nl(out, obj);

        json_key(out, obj, "body");
        fprintf(out, "\""); json_escape(out, sb_to_sv(text)); fprintf(out, "\"");
        json_nl(out, fmt);

        json_indent(out, inner);
        fprintf(out, "}");

        sb_free(text);
    }

    json_nl(out, fmt);
    json_indent(out, fmt);
    fprintf(out, "]");
}

// TODO: maybe we can have more encoder for this.
static void json_write_attachments(FILE *out, const char *path, Json_Fmt fmt, bool compress)
{
    fprintf(out, "[");
    json_nl(out, fmt);

    File_Paths files = {0};
    bool first = true;

    if (fs_read_dir(path, &files) && files.count > 0) {
        Json_Fmt inner = fmt;
        inner.indent++;

        for (size_t i = 0; i < files.count; i++) {
            const char *name = files.items[i];

            char fullpath[1024];
            snprintf(fullpath, sizeof(fullpath), "%s/%s", path, name);

            FILE *f = fopen(fullpath, "rb");
            if (!f) continue;

            fseek(f, 0, SEEK_END);
            size_t size = (size_t)ftell(f);
            rewind(f);

            unsigned char *buf = malloc(size > 0 ? size : 1);
            if (!buf) {
                fclose(f);
                continue;
            }

            size = fread(buf, 1, size, f);
            fclose(f);

            if (!first) {
                fprintf(out, ",");
                json_nl(out, fmt);
            }

            json_indent(out, inner);
            fprintf(out, "{");
            json_nl(out, fmt);

            Json_Fmt obj = inner;
            obj.indent++;

            json_key(out, obj, "name");
            fprintf(out, "\"");
            json_escape(out, sv_from_cstr(name));
            fprintf(out, "\",");
            json_nl(out, obj);

            const char *mime = mime_from_buf(buf, size);
            bool is_text = mime_is_text(mime);

            json_key(out, obj, "type");
            fprintf(out, "\"%s\",", mime);
            json_nl(out, obj);

            json_key(out, obj, "encoding");
            fprintf(out, "\"%s\",", (is_text && !compress) ? "utf-8" : "base64");
            json_nl(out, obj);

            json_key(out, obj, "data");
            fprintf(out, "\"");

            if (is_text && !compress) {
                json_escape(out, sv_from_parts((const char*)buf, size));
            } else {
                String_Builder sb = {0};
                b64_encode(&sb, buf, size);
                fprintf(out, SV_Fmt, SB_Arg(sb));
                sb_free(sb);
            }

            fprintf(out, "\"");
            json_nl(out, fmt);

            json_indent(out, inner);
            fprintf(out, "}");

            free(buf);
            first = false;
        }
    }

    if (files.items) da_free(files);

    json_nl(out, fmt);
    json_indent(out, fmt);
    fprintf(out, "]");
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
    fprintf(out, "\""); json_escape(out, issue_status_to_sv(iss->status)); fprintf(out, "\","); json_nl(out, fmt);

    json_key(out, fmt, "priority");
    fprintf(out, "\""); json_escape(out, issue_priority_to_sv(iss->priority)); fprintf(out, "\","); json_nl(out, fmt);

    json_key(out, fmt, "tags");
    json_write_tags(out, iss->tags, fmt);
    fprintf(out, ","); json_nl(out, fmt);

    json_key(out, fmt, "created");
    fprintf(out, "\""); json_escape(out, iss->created); fprintf(out, "\","); json_nl(out, fmt);

    json_key(out, fmt, "body");
    fprintf(out, "\""); json_escape(out, iss->body); fprintf(out, "\","); json_nl(out, fmt);

    json_key(out, fmt, "comments");
    json_write_comments(out, iss->body, fmt);
    fprintf(out, ","); json_nl(out, fmt);

    json_key(out, fmt, "attachments");
    json_write_attachments(out, iss->attach_path, fmt, opts->compress);
    json_nl(out, fmt);

    fmt.indent--;
    json_indent(out, fmt);
    fprintf(out, "}"); json_nl(out, fmt);
}
