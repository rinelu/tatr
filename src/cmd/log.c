#include "cmd.h"
#include "tatrlog.h"

#include <stdio.h>
#include <string.h>
#include <time.h>

static bool is_header_field(String_View key)
{
    return sv_eq_cstr(key, "event") ||
           sv_eq_cstr(key, "time")  ||
           sv_eq_cstr(key, "id");
}

static void print_entry_full(const TatrLog_Entry *e)
{
    char rel[32], abs[128];
    ui_format_time_relative(e->time, rel, sizeof(rel));
    human_timestamp(e->time, abs, sizeof(abs));

    const char *evname = tatrlog_event_name(e->event);

    // Line 1: event  id  (relative)
    printf("%s%s%s  %s"SV_Fmt"%s  %s(%s)%s\n",
           log_seq(ui_event_color(e->event)), evname,   log_seq(A_RESET),
           log_seq(A_BYELLOW),  SV_Arg(e->id),          log_seq(A_RESET),
           log_seq(A_DIM),      rel,                    log_seq(A_RESET));

    // Line 2: full timestamp, indented to align with id
    printf("%*s  %s%s%s\n",
           (int)strlen(evname), "",
           log_seq(A_DIM), abs, log_seq(A_RESET));

    // Body fields
    da_foreach(TatrLog_Field, f, &e->fields) {
        if (is_header_field(f->key)) continue;
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

static void print_entry_oneline(const TatrLog_Entry *e)
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

static bool matches_event(const TatrLog_Entry *e, const char *f)
{
    if (!f || !*f) return true;
    return e->event == tatrlog_event_from_str(f);
}

static bool matches_id(const TatrLog_Entry *e, const char *id)
{
    if (!id || !*id) return true;
    return strncmp(id, e->id.data, id[0] ? strlen(id) : 0) == 0;
}

static bool matches_since(const TatrLog_Entry *e, time_t t)
{
    return t == 0 || e->time >= t;
}

static bool matches_until(const TatrLog_Entry *e, time_t t)
{
    return t == 0 || e->time <= t;
}

static bool matches_author(const TatrLog_Entry *e, const char *a)
{
    if (!a || !*a) return true;
    String_View sv = tatrlog_get(e, "author");
    return sv.data && sv_icontains(sv, sv_from_cstr(a));
}

int cmd_log(int argc, char **argv)
{
    uint64_t *limit   = clag_uint64("limit",   'n', 0,    "Max entries (0 = all)");
    char    **since   = clag_str   ("since",    0,  NULL, "After  date (YYYY-MM-DD or ISO)");
    char    **until   = clag_str   ("until",    0,  NULL, "Before date (YYYY-MM-DD or ISO)");
    char    **id_flag = clag_str   ("id",       0,  NULL, "Filter to issue ID (prefix match)");
    char    **event   = clag_str   ("event",   'e', NULL, "Filter by event type");
    char    **author  = clag_str   ("author",  'a', NULL, "Filter by author");
    bool     *oneline = clag_bool  ("oneline", 'l', false,"Compact one-line output");
    bool     *reverse = clag_bool  ("reverse", 'r', false,"Oldest entries first");

    clag_usage("[<id>] [options]");
    clag_choices("event",
        "create", "edit",    "close",  "reopen",
        "delete", "tag",     "comment","attach", "detach");

    if (!clag_parse(argc, argv)) {
        clag_print_error(stderr);
        clag_print_options(stderr);
        return 1;
    }
    if (!require_repo()) return 1;

    const char *id_filter = *id_flag;
    if (!id_filter && clag_rest_argc() > 0)
        id_filter = clag_rest_argv()[0];

    time_t t_since = parse_date(*since);
    time_t t_until = parse_date(*until);

    TatrLog_Entries entries = {0};
    if (!tatrlog_load(&entries)) {
        log_error("no log found — make some changes first");
        return 1;
    }
    if (entries.count == 0) {
        log_msg("(no log entries)");
        tatrlog_entries_free(&entries);
        return 0;
    }

    size_t   total = entries.count;
    uint64_t shown = 0;

    long start = *reverse ? 0           : (long)total - 1;
    long end   = *reverse ? (long)total : -1;
    long step  = *reverse ? 1           : -1;

    for (long i = start; i != end; i += step) {
        const TatrLog_Entry *e = &entries.items[i];

        if (!matches_event (e, *event))    continue;
        if (!matches_id    (e, id_filter)) continue;
        if (!matches_since (e, t_since))   continue;
        if (!matches_until (e, t_until))   continue;
        if (!matches_author(e, *author))   continue;

        if (*oneline) print_entry_oneline(e);
        else          print_entry_full(e);

        shown++;
        if (*limit > 0 && shown >= *limit) break;
    }

    if (shown == 0) log_msg("(no matching log entries)");

    tatrlog_entries_free(&entries);
    return 0;
}
