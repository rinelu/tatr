#include "render.h"

#include "log.h"
#include "ui.h"
#include "temp.h"

#include <stdio.h>
#include <string.h>
#include <inttypes.h>

#define COL_ID        24
#define COL_STATUS    8
#define COL_PRIORITY  10
#define COL_GAP       2

// TODO: Move ui.h/c into this

static void human_issue_list(FILE *out, const Tatr_Issue_List_Result *r)
{
    (void)out;
    int tw = term_width();
    int indent = COL_ID + COL_STATUS + COL_PRIORITY + 2 * COL_GAP;
    int max_title = tw - indent;

    if (tw < 70) {
        log_error("Terminal window is too small.");
        return;
    }

    if (r->show_header)
        log_msg("%s%-*s  %-*s  %-*s  %s%s",
            log_seq(A_BOLD_WHITE),
            COL_ID, "ID",
            COL_STATUS, "STATUS",
            COL_PRIORITY, "PRIORITY",
            "TITLE",
            log_seq(A_RESET));

    if (max_title < 8) max_title = 8;
    if (max_title > 255) max_title = 255;

    for (size_t i = 0; i < r->issues.count; i++) {
        const Tatr_Issue_Summary *iss = &r->issues.items[i];

        printf("%s%-*.*s%s  ",
            log_seq(A_BYELLOW), COL_ID, SV_Arg(iss->id), log_seq(A_RESET));

        printf("%s%-*s%s  ",
            log_seq(ui_status_color(iss->status)), COL_STATUS,
            issue_status_to_cstr(iss->status), log_seq(A_RESET));

        printf("%s%-*s%s  ",
            log_seq(ui_priority_color(iss->priority)), COL_PRIORITY,
            issue_priority_to_cstr(iss->priority), log_seq(A_RESET));

        ui_wrap(stdout, temp_sv_to_cstr(iss->title), indent, indent + 2, tw);
    }

    if (r->issues.count == 0) log_msg("(no issues)");
}

static void human_issue_view(FILE *out, const Tatr_Issue_View *v)
{
    (void)out;

    if (v->raw_mode) {
        log_msg(SV_Fmt, SV_Arg(v->raw));
        return;
    }

    log_msg("%sissue %s"SV_Fmt"%s", log_seq(A_BYELLOW), log_seq(A_BOLD), SV_Arg(v->id), log_seq(A_RESET));

#define LABEL(s) printf("%s%-10s%s", log_seq(A_BOLD_WHITE), (s), log_seq(A_RESET))

    LABEL("Author:");
    log_msg("  "SV_Fmt, SV_Arg(v->created));

    LABEL("Status:");
    log_msg("  %s%s%s",
        log_seq(ui_status_color(v->status)),
        issue_status_to_cstr(v->status),
        log_seq(A_RESET));

    LABEL("Priority:");
    log_msg("  %s%s%s",
            log_seq(ui_priority_color(v->priority)),
            issue_priority_to_cstr(v->priority),
            log_seq(A_RESET));

    if (v->tags.count)
        LABEL("Tags:"),
        log_msg("  %s"SV_Fmt"%s", log_seq(A_CYAN), SV_Arg(v->tags), log_seq(A_RESET));

#undef LABEL

    log_msg("\n    %s%.*s%s", log_seq(A_BOLD), SV_Arg(v->title), log_seq(A_RESET));

    if (v->body.count) {
        String_View cursor = v->body, line;

        while (cursor.count > 0) {
            line = sv_slice_by_delim(&cursor, '\n');

            String_View trimmed = sv_trim(line);
            if (sv_eq(trimmed, sv_from_cstr("---comment---"))) {
                print_rule();
                continue;
            }

            log_msg("    "SV_Fmt, SV_Arg(line));
        }
    }

    fputc('\n', stdout);

    if (v->has_attachments_dir) {
        log_msg("\nAttachments (%zu):", v->attachment_count);
        for (size_t i = 0; i < v->attachment_count; i++)
            log_msg("  %s", v->attachments[i]);
    }
}

static void human_attachment_list(FILE *out, const Tatr_Attachment_List_Result *r)
{
    (void)out;
    if (r->count == 0) {
        log_msg("(no attachments)");
        return;
    }

    log_msg("Attachments of issue '"SV_Fmt"'", SV_Arg(r->issue_id));
    for (size_t i = 0; i < r->count; i++)
        log_msg("  - %s", r->items[i]);
}

static void human_search_result(FILE *out, const Tatr_Search_Result *r)
{
    (void)out;
    for (size_t i = 0; i < r->matches.count; i++) {
        const Tatr_Issue_Summary *iss = &r->matches.items[i];
        log_msg("%s"SV_Fmt"%s  %s(%s)%s  "SV_Fmt,
            log_seq(A_BYELLOW), SV_Arg(iss->id), log_seq(A_RESET),
            log_seq(ui_status_color(iss->status)),
            issue_status_to_cstr(iss->status), log_seq(A_RESET),
            SV_Arg(iss->title));
    }

    if (r->matches.count == 0) {
        log_msg("(no results)");
        return;
    }

    log_msg("(%zu result%s for '%s')",
        r->matches.count, r->matches.count == 1 ? "" : "s", r->query);
}

static bool human__log_is_header_field(String_View key)
{
    return sv_eq_cstr(key, "event") ||
           sv_eq_cstr(key, "time")  ||
           sv_eq_cstr(key, "id");
}

static void human__log_entry_full(const TatrLog_Entry *e)
{
    char rel[32], abs[128];
    ui_format_time_relative(e->time, rel, sizeof(rel));
    human_timestamp(e->time, abs, sizeof(abs));

    const char *evname = tatrlog_event_name(e->event);

    printf("%s%s%s  %s"SV_Fmt"%s  %s(%s)%s\n",
           log_seq(ui_event_color(e->event)), evname,   log_seq(A_RESET),
           log_seq(A_BYELLOW),  SV_Arg(e->id),          log_seq(A_RESET),
           log_seq(A_DIM),      rel,                    log_seq(A_RESET));

    printf("%*s  %s%s%s\n",
           (int)strlen(evname), "",
           log_seq(A_DIM), abs, log_seq(A_RESET));

    da_foreach(TatrLog_Field, f, &e->fields) {
        if (human__log_is_header_field(f->key)) continue;
        printf("    %s"SV_Fmt":%s%*s"SV_Fmt"\n",
               log_seq(A_BOLD_WHITE), SV_Arg(f->key), log_seq(A_RESET),
               (int)(10 - (int)f->key.count), "",
               SV_Arg(f->val));
    }

    if (e->body.count > 0) {
        String_View cursor = e->body;
        while (!sv_empty(cursor)) {
            String_View line = sv_slice_by_delim(&cursor, '\n');
            printf("    %s"SV_Fmt"%s\n",
                   log_seq(A_DIM), SV_Arg(line), log_seq(A_RESET));
        }
        putchar('\n');
    }

    putchar('\n');
}

static void human__log_entry_oneline(const TatrLog_Entry *e)
{
    char rel[32];
    ui_format_time_relative(e->time, rel, sizeof(rel));

    String_View title = tatrlog_get(e, "title");

    printf("%s%-8s%s  %s"SV_Fmt"%s  %s%s%s",
           log_seq(ui_event_color(e->event)),
           tatrlog_event_name(e->event),
           log_seq(A_RESET),
           log_seq(A_BYELLOW), SV_Arg(e->id), log_seq(A_RESET),
           log_seq(A_DIM), rel, log_seq(A_RESET));

    if (title.count > 0)
        printf("  "SV_Fmt, SV_Arg(title));

    putchar('\n');
}

static void human_log_result(FILE *out, const Tatr_Log_Result *r)
{
    (void)out;
    for (size_t i = 0; i < r->count; i++) {
        if (r->oneline) human__log_entry_oneline(r->items[i]);
        else            human__log_entry_full(r->items[i]);
    }

    if (r->count == 0) log_msg("(no matching log entries)");
}

static void human_status(FILE *out, const Tatr_Status_Result *r)
{
    (void)out;

    log_msg("%zu issue%s  --  %s%zu open%s  %s%zu in-progress%s  %s%zu closed%s  %s(%.0f%% complete)%s",
        r->total, r->total == 1 ? "" : "s",
        A_GREEN, r->open, A_RESET,
        A_BLUE, r->in_progress, A_RESET,
        A_DIM, r->closed, A_RESET,
        A_DIM, r->total > 0 ? (double)r->closed / (double)r->total * 100.0 : 0.0, A_RESET);

    if (r->high_priority_count == 0) {
        log_msg("\n%sHigh priority:%s %s(none)%s", A_BOLD, A_RESET, A_DIM, A_RESET);
    } else {
        log_msg("\n%sHigh priority:%s", A_BOLD_RED, A_RESET);
        for (size_t i = 0; i < r->high_priority_count; i++) {
            const Tatr_Status_Issue *iss = &r->high_priority[i];
            const char *pcol = iss->priority == ISSUE_PCRITICAL ? A_BOLD_RED : A_YELLOW;
            log_msg("  %s%s%s  "SV_Fmt, pcol, iss->id, A_RESET, SV_Arg(iss->title));
        }
    }

    if (r->stale_count == 0) {
        log_msg("\n%sStale:%s %s(none)%s", A_BOLD, A_RESET, A_DIM, A_RESET);
    } else {
        log_msg("\n%sStale%s  %s(no activity >%"PRIu64" days):%s",
                A_BOLD_YELLOW, A_RESET, A_DIM, r->stale_days, A_RESET);
        for (size_t i = 0; i < r->stale_count; i++) {
            const Tatr_Status_Issue *iss = &r->stale[i];
            log_msg("  %s%s%s  "SV_Fmt"  %s(%s)%s",
                    A_YELLOW, iss->id, A_RESET,
                    SV_Arg(iss->title),
                    A_DIM, iss->updated_relative, A_RESET);
        }
    }

    log_msg("\n%sRecent activity:%s", A_BOLD, A_RESET);
    if (r->recent_count == 0) {
        log_msg("  %s(no issues)%s", A_DIM, A_RESET);
    } else {
        for (size_t i = 0; i < r->recent_count; i++) {
            const Tatr_Status_Issue *iss = &r->recent[i];
            log_msg("  %s%s%s  "SV_Fmt"  %s%s%s",
                   A_DIM, iss->id, A_RESET,
                   SV_Arg(iss->title),
                   A_DIM, iss->updated_relative, A_RESET);
        }
    }

    log_msg("\n%sTop tags:%s", A_BOLD, A_RESET);
    if (r->top_tags_count == 0) {
        log_msg("%s  (none)%s", A_DIM, A_RESET);
    } else {
        String_Builder row = {0};
        for (size_t i = 0; i < r->top_tags_count; i++) {
            if (i > 0) sb_append_cstr(&row, "    ");
            sb_appendf(&row, "%s"SV_Fmt"%s  %sx%zu%s",
                    A_CYAN, SV_Arg(r->top_tags[i].tag), A_RESET,
                    A_DIM, r->top_tags[i].count, A_RESET);
        }
        sb_append_null(&row);
        log_msg("  %s", row.items);
        sb_free(row);
    }

    printf("\n");
}

static void human_config_list(FILE *out, const Tatr_Config_List_Result *r)
{
    (void)out;

    if (r->show_local && r->local && r->local->entries.count > 0) {
        printf("%s%s%s  %s(%s)%s\n",
               log_seq(A_BOLD), "local", log_seq(A_RESET),
               log_seq(A_DIM),  r->local->path, log_seq(A_RESET));
        da_foreach(Config_Entry, e, &r->local->entries) {
            bool known = config_key_def(temp_strndup(e->key.data, e->key.count)) != NULL;
            printf("  %s"SV_Fmt"%s = %s"SV_Fmt"%s%s\n",
                   log_seq(known ? A_BOLD_WHITE : A_YELLOW),
                   SV_Arg(e->key), log_seq(A_RESET),
                   log_seq(A_DIM), SV_Arg(e->val), log_seq(A_RESET),
                   known ? "" : "  (unknown key)");
        }
        putchar('\n');
    }

    if (r->show_global && r->global && r->global->entries.count > 0) {
        printf("%s%s%s  %s(%s)%s\n",
               log_seq(A_BOLD), "global", log_seq(A_RESET),
               log_seq(A_DIM),  r->global->path, log_seq(A_RESET));
        da_foreach(Config_Entry, e, &r->global->entries) {
            bool known = config_key_def(temp_strndup(e->key.data, e->key.count)) != NULL;
            printf("  %s"SV_Fmt"%s = %s"SV_Fmt"%s%s\n",
                   log_seq(known ? A_BOLD_WHITE : A_YELLOW),
                   SV_Arg(e->key), log_seq(A_RESET),
                   log_seq(A_DIM), SV_Arg(e->val), log_seq(A_RESET),
                   known ? "" : "  (unknown key)");
        }
        putchar('\n');
    }

    if (r->show_resolved) {
        printf("%sresolved%s\n", log_seq(A_BOLD), log_seq(A_RESET));
        for (size_t i = 0; i < r->resolved_count; i++) {
            printf("  %s%-20s%s = %s  %s(%s)%s\n",
                   log_seq(A_BOLD_WHITE), r->resolved[i].key, log_seq(A_RESET),
                   r->resolved[i].val,
                   log_seq(A_DIM), r->resolved[i].source, log_seq(A_RESET));
        }
    }
}

static void human_config_keys(FILE *out, const Tatr_Config_Keys_Result *r)
{
    (void)out;
    printf("%sAvailable keys:%s\n\n", log_seq(A_BOLD), log_seq(A_RESET));
    for (size_t i = 0; i < r->count; i++) {
        const Config_Key_Def *d = &r->keys[i];
        printf("  %s%-20s%s %s\n",
               log_seq(A_BOLD_WHITE), d->key, log_seq(A_RESET),
               d->desc);
        if (d->default_val)
            printf("  %s%-20s%s default: %s\n",
                   log_seq(A_DIM), "", log_seq(A_RESET),
                   d->default_val);
    }
}

static void human_message(FILE *out, const char *msg)
{
    (void)out;
    log_info("%s", msg);
}

static void human_error(FILE *out, Tatr_Error err, const char *context)
{
    (void)out;
    if (context) log_error("%s: %s", context, tatr_strerror(err));
    else         log_error("%s", tatr_strerror(err));
}

const Renderer RENDER_HUMAN = {
    .issue_view      = human_issue_view,
    .issue_list      = human_issue_list,
    .attachment_list = human_attachment_list,
    .search_result   = human_search_result,
    .log_result      = human_log_result,
    .status          = human_status,
    .config_list     = human_config_list,
    .config_keys     = human_config_keys,
    .message         = human_message,
    .error           = human_error,
};
