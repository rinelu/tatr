#include "cmd.h"
#include "tatr.h"

int cmd_search(int argc, char **argv)
{
    bool     *case_flag   = clag_bool  ("case",   'c', false, "Case-sensitive match");
    bool     *header_only = clag_bool  ("header", 'H', false, "Search header only");
    uint64_t *limit       = clag_uint64("limit",  'n', 0,     "Max results (0 = all)");
    clag_usage("<query...> [options]");

    if (!clag_parse(argc, argv)) {
        clag_print_error(stderr);
        return 1;
    }
    if (!require_repo()) return 1;

    if (clag_rest_argc() < 1) {
        log_error("Missing search query");
        return 1;
    }

    Temp_Checkpoint tmark = temp_save();
    const Renderer *r = renderer_for(TATR_FMT_HUMAN);

    Tatr_Search_Query q = {
        .tokens         = (const char *const *)clag_rest_argv(),
        .token_count    = (size_t)clag_rest_argc(),
        .case_sensitive = *case_flag,
        .header_only    = *header_only,
        .limit          = *limit,
    };

    Tatr_Search_Result result;
    Tatr_Error err = tatr_issue_search(&q, &result);

    if (err != TATR_OK) {
        log_error("Cannot read issues directory");
        temp_rewind(tmark);
        return 1;
    }

    r->search_result(stdout, &result);

    temp_rewind(tmark);
    return result.matches.count > 0 ? 0 : 1;
}
