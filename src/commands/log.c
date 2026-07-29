#include "cmd.h"
#include "tatr.h"

#include <time.h>

int cmd_log(int argc, char **argv)
{
    Config cfg = {0};
    config_load(&cfg);
    uint64_t def_limit = (uint64_t)config_get_int(&cfg, "log.limit");
    config_free(&cfg);

    uint64_t *limit   = clag_uint64("limit",   'n', def_limit,    "Max entries (0 = all)");
    char    **since   = clag_str   ("since",    0,  NULL,   "After  date (YYYY-MM-DD or ISO)");
    char    **until   = clag_str   ("until",    0,  NULL,   "Before date (YYYY-MM-DD or ISO)");
    char    **id_flag = clag_str   ("id",       0,  NULL,   "Filter to issue ID (prefix match)");
    char    **event   = clag_str   ("event",   'e', NULL,   "Filter by event type");
    char    **author  = clag_str   ("author",  'a', NULL,   "Filter by author");
    bool     *oneline = clag_bool  ("oneline", 'l', false,  "Compact one-line output");
    bool     *reverse = clag_bool  ("reverse", 'r', false,  "Oldest entries first");

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

    Temp_Checkpoint tmark = temp_save();
    const Renderer *r = renderer_for(TATR_FMT_HUMAN);

    Tatr_Log_Query q = {
        .id_prefix = id_filter,
        .event     = *event,
        .author    = *author,
        .since     = parse_date(*since),
        .until     = parse_date(*until),
        .reverse   = *reverse,
        .oneline   = *oneline,
        .limit     = *limit,
    };

    TatrLog_Entries entries = {0};
    Tatr_Log_Result result;
    Tatr_Error err = tatr_log_query(&q, &entries, &result);

    if (err != TATR_OK) {
        log_error("no log found — make some changes first");
        temp_rewind(tmark);
        return 1;
    }

    if (entries.count == 0) {
        log_msg("(no log entries)");
        tatrlog_entries_free(&entries);
        temp_rewind(tmark);
        return 0;
    }

    r->log_result(stdout, &result);

    tatrlog_entries_free(&entries);
    temp_rewind(tmark);
    return 0;
}
