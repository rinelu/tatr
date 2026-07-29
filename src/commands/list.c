#include "cmd.h"
#include "tatr.h"
#include <stdint.h>
#include <stdlib.h>

typedef struct {
    Tatr_Issue_Filter filter;
    bool show_header;
} ListOptions;

static bool list__parse_opts(int argc, char **argv, ListOptions *opt)
{
    Config cfg = {0};
    config_load(&cfg);
    bool     def_all   = config_get_bool(&cfg, "list.show_closed");
    uint64_t def_limit  = (uint64_t)config_get_int(&cfg, "list.limit");
    config_free(&cfg);

    char    **status   = clag_str ("status",   's', NULL,      "Filter by status");
    char    **priority = clag_str ("priority", 'p', NULL,      "Filter by priority");
    char    **tag      = clag_str ("tag",      'T', NULL,      "Filter by tag");
    bool     *all      = clag_bool("all",      'a', def_all,   "Include closed issues");
    bool     *nohdr    = clag_bool("no-header",'q', false,     "Suppress column header");
    uint64_t *limit    = clag_uint64("limit",  'n', def_limit, "Max issues to show (0 = all)");

    clag_usage("[options]");

    if (!clag_parse(argc, argv)) {
        clag_print_error(stderr);
        return false;
    }

    opt->filter.status         = issue_status_from_cstr(*status);
    opt->filter.priority       = issue_priority_from_cstr(*priority);
    opt->filter.tag            = *tag;
    opt->filter.include_closed = *all;
    opt->filter.limit          = *limit;

    opt->show_header = !*nohdr;

    return true;
}

int cmd_list(int argc, char **argv)
{
    ListOptions opt = {0};
    if (!list__parse_opts(argc, argv, &opt)) return 1;
    if (!require_repo()) return 1;

    Temp_Checkpoint tmark = temp_save();

    Tatr_Issue_List_Result result;
    Tatr_Error err = tatr_issue_list(&opt.filter, opt.show_header, &result);

    const Renderer *r = renderer_for(TATR_FMT_HUMAN);
    if (err != TATR_OK) {
        r->error(stdout, err, "cannot read issues directory");
        temp_rewind(tmark);
        return 1;
    }

    r->issue_list(stdout, &result);

    temp_rewind(tmark);
    return 0;
}
