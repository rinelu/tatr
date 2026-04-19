#include "cmd.h"
#include "util.h"

static void print_entry_full(const TatrLog_Entry *e)
{
    char time_rel[32];
    char time_full[32];
    ui_format_time_relative(e->time, time_rel, sizeof(time_rel));
    get_timestamp(e->time, time_full, sizeof(time_full));

    // Header line: colored event + id
    printf("%s%-8s%s  %s"SV_Fmt"%s",
           log_seq(ui_event_color(e->event)),
           tatrlog_event_name(e->event),
           log_seq(A_RESET),
           log_seq(A_BYELLOW),
           SV_Arg(e->id),
           log_seq(A_RESET));

    // Timestamp
    printf("  %s%s  (%s)%s\n",
           log_seq(A_DIM),
           time_full,
           time_rel,
           log_seq(A_RESET));

    if (e->detail.count > 0)
        log_msg("         %s"SV_Fmt"%s",
               log_seq(A_DIM),
               SV_Arg(e->detail),
               log_seq(A_RESET));
}

static void print_entry_oneline(const TatrLog_Entry *e)
{
    char time_rel[32];
    ui_format_time_relative(e->time, time_rel, sizeof(time_rel));

    printf("%s%-8s%s  %s"SV_Fmt"%s  %s%s%s",
           log_seq(ui_event_color(e->event)),
           tatrlog_event_name(e->event),
           log_seq(A_RESET),
           log_seq(A_BYELLOW),
           SV_Arg(e->id),
           log_seq(A_RESET),
           log_seq(A_DIM),
           time_rel,
           log_seq(A_RESET));

    if (e->detail.count > 0)
        printf("  "SV_Fmt, SV_Arg(e->detail));

    putchar('\n');
}

static bool matches_event(const TatrLog_Entry *e, const char *filter)
{
    if (!filter || !*filter) return true;
    return strcmp(tatrlog_event_name(e->event), filter) == 0;
}

static bool matches_id(const TatrLog_Entry *e, const char *id)
{
    if (!id || !*id) return true;
    return sv_eq_cstr(e->id, id);
}

static bool matches_since(const TatrLog_Entry *e, time_t since)
{
    if (since == 0) return true;
    return e->time >= since;
}

static bool matches_until(const TatrLog_Entry *e, time_t until)
{
    if (until == 0) return true;
    return e->time <= until;
}

int cmd_log(int argc, char **argv)
{
    uint64_t *limit   = clag_uint64("limit",   'n', 0,     "Max entries to show (0 = all)");
    char    **since   = clag_str   ("since",   0,   NULL,  "Show entries after date (YYYY-MM-DD or ISO)");
    char    **until   = clag_str   ("until",   0,   NULL,  "Show entries before date (YYYY-MM-DD or ISO)");
    char    **id_flag = clag_str   ("id",      0,   NULL,  "Filter to a specific issue ID");
    char    **event   = clag_str   ("event",   'e', NULL,  "Filter by event type");
    bool     *oneline = clag_bool  ("oneline", 'l', NULL,  "Compact single-line output");
    bool     *reverse = clag_bool  ("reverse", 'r', NULL,  "Show oldest entries first");

    clag_usage("[<id>] [options]");
    clag_choices("event", "create", "edit", "close", "reopen", "delete",
                          "tag", "comment", "attach", "detach");

    if (!clag_parse(argc, argv)) {
        clag_print_error(stderr);
        return 1;
    }
    if (!require_repo()) return 1;

    // Positional arg is shorthand for --id
    const char *id_filter = *id_flag;
    if (!id_filter && clag_rest_argc() > 0)
        id_filter = clag_rest_argv()[0];

    time_t t_since = parse_date(*since);
    time_t t_until = parse_date(*until);

    TatrLog_Entries entries = {0};
    if (!tatrlog_load(&entries)) {
        log_error("no log found.");
        return 1;
    }

    if (entries.count == 0) {
        log_msg("(no log entries)");
        tatrlog_entries_free(&entries);
        return 0;
    }

    size_t total = entries.count;
    uint64_t shown = 0;

    long start = *reverse ? 0           : (long)total - 1;
    long end   = *reverse ? (long)total : -1;
    long step  = *reverse ? 1           : -1;

    for (long i = start; i != end; i += step) {
        const TatrLog_Entry *e = &entries.items[i];

        if (!matches_event(e, *event))    continue;
        if (!matches_id   (e, id_filter)) continue;
        if (!matches_since(e, t_since))   continue;
        if (!matches_until(e, t_until))   continue;

        if (*oneline) print_entry_oneline(e);
        else          print_entry_full(e);

        shown++;
        if (*limit > 0 && shown >= *limit) break;
    }

    if (shown == 0) log_msg("(no matching log entries)");

    tatrlog_entries_free(&entries);
    return 0;
}
