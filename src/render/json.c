#include "render.h"

#include "issue.h"

#include <stdio.h>

static void json_escape(FILE *out, String_View sv)
{
    for (size_t i = 0; i < sv.count; i++) {
        unsigned char c = (unsigned char)sv.data[i];
        switch (c) {
            case '"':  fputs("\\\"", out); break;
            case '\\': fputs("\\\\", out); break;
            case '\n': fputs("\\n",  out); break;
            case '\r': fputs("\\r",  out); break;
            case '\t': fputs("\\t",  out); break;
            default:
                if (c < 0x20) fprintf(out, "\\u%04x", c);
                else          fputc((int)c, out);
        }
    }
}

static void json_str(FILE *out, String_View sv)
{
    fputc('"', out);
    json_escape(out, sv);
    fputc('"', out);
}

static void json_issue_list(FILE *out, const Tatr_Issue_List_Result *r)
{
    fputc('[', out);
    for (size_t i = 0; i < r->issues.count; i++) {
        const Tatr_Issue_Summary *iss = &r->issues.items[i];
        if (i > 0) fputc(',', out);
        fputs("{\"id\":", out);      json_str(out, iss->id);
        fputs(",\"title\":", out);   json_str(out, iss->title);
        fputs(",\"status\":", out);  json_str(out, issue_status_to_sv(iss->status));
        fputs(",\"priority\":", out); json_str(out, issue_priority_to_sv(iss->priority));
        fputc('}', out);
    }
    fputc(']', out);
    fputc('\n', out);
}

static void json_issue_view(FILE *out, const Tatr_Issue_View *v)
{
    fputs("{\"id\":", out);       json_str(out, v->id);
    fputs(",\"title\":", out);    json_str(out, v->title);
    fputs(",\"status\":", out);   json_str(out, issue_status_to_sv(v->status));
    fputs(",\"priority\":", out); json_str(out, issue_priority_to_sv(v->priority));
    fputs(",\"created\":", out);  json_str(out, v->created);
    fputs(",\"tags\":", out);     json_str(out, v->tags);
    fputs(",\"body\":", out);     json_str(out, v->raw_mode ? v->raw : v->body);

    fputs(",\"attachments\":[", out);
    for (size_t i = 0; i < v->attachment_count; i++) {
        if (i > 0) fputc(',', out);
        json_str(out, sv_from_cstr(v->attachments[i]));
    }
    fputs("]}\n", out);
}

static void json_attachment_list(FILE *out, const Tatr_Attachment_List_Result *r)
{
    fputs("{\"issue_id\":", out); json_str(out, r->issue_id);
    fputs(",\"attachments\":[", out);
    for (size_t i = 0; i < r->count; i++) {
        if (i > 0) fputc(',', out);
        json_str(out, sv_from_cstr(r->items[i]));
    }
    fputs("]}\n", out);
}

static void json_search_result(FILE *out, const Tatr_Search_Result *r)
{
    fputs("{\"query\":", out); json_str(out, sv_from_cstr(r->query));
    fputs(",\"matches\":[", out);
    for (size_t i = 0; i < r->matches.count; i++) {
        const Tatr_Issue_Summary *iss = &r->matches.items[i];
        if (i > 0) fputc(',', out);
        fputs("{\"id\":", out);     json_str(out, iss->id);
        fputs(",\"title\":", out);  json_str(out, iss->title);
        fputs(",\"status\":", out); json_str(out, issue_status_to_sv(iss->status));
        fputc('}', out);
    }
    fputs("]}\n", out);
}

static void json_log_result(FILE *out, const Tatr_Log_Result *r)
{
    fputc('[', out);
    for (size_t i = 0; i < r->count; i++) {
        const TatrLog_Entry *e = r->items[i];
        if (i > 0) fputc(',', out);
        fputs("{\"event\":", out); json_str(out, sv_from_cstr(tatrlog_event_name(e->event)));
        fputs(",\"id\":", out);    json_str(out, e->id);
        fputc('}', out);
    }
    fputs("]\n", out);
}

static void json_status(FILE *out, const Tatr_Status_Result *r)
{
    fprintf(out,
        "{\"total\":%zu,\"open\":%zu,\"in_progress\":%zu,\"closed\":%zu}\n",
        r->total, r->open, r->in_progress, r->closed);
}

static void json_config_list(FILE *out, const Tatr_Config_List_Result *r)
{
    fputc('{', out);
    for (size_t i = 0; i < r->resolved_count; i++) {
        if (i > 0) fputc(',', out);
        json_str(out, sv_from_cstr(r->resolved[i].key));
        fputc(':', out);
        json_str(out, sv_from_cstr(r->resolved[i].val));
    }
    fputs("}\n", out);
}

static void json_config_keys(FILE *out, const Tatr_Config_Keys_Result *r)
{
    fputc('[', out);
    for (size_t i = 0; i < r->count; i++) {
        if (i > 0) fputc(',', out);
        json_str(out, sv_from_cstr(r->keys[i].key));
    }
    fputs("]\n", out);
}

static void json_message(FILE *out, const char *msg)
{
    fputs("{\"message\":", out);
    json_str(out, sv_from_cstr(msg));
    fputs("}\n", out);
}

static void json_error(FILE *out, Tatr_Error err, const char *context)
{
    fputs("{\"error\":", out);
    json_str(out, sv_from_cstr(tatr_strerror(err)));
    if (context) {
        fputs(",\"context\":", out);
        json_str(out, sv_from_cstr(context));
    }
    fputs("}\n", out);
}

const Renderer RENDER_JSON = {
    .issue_view      = json_issue_view,
    .issue_list      = json_issue_list,
    .attachment_list = json_attachment_list,
    .search_result   = json_search_result,
    .log_result      = json_log_result,
    .status          = json_status,
    .config_list     = json_config_list,
    .config_keys     = json_config_keys,
    .message         = json_message,
    .error           = json_error,
};
