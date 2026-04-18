#include "cmd.h"

typedef struct {
    const char *status;
    const char *priority;
    const char *tag;
    bool include_closed;
} IssueFilter;

static bool list__match(const Issue *iss, const IssueFilter *f)
{
    if (!f->include_closed && issue_is_closed(iss))
        return false;

    if (f->status && !sv_eq(iss->status, sv_from_cstr(f->status)))
        return false;

    if (f->priority && !sv_eq(iss->priority, sv_from_cstr(f->priority)))
        return false;

    if (f->tag && !issue_has_tag(iss, f->tag))
        return false;

    return true;
}

typedef struct {
    IssueFilter filter;
    bool show_header;
    uint64_t limit;
} ListOptions;

static bool list__parse_opts(int argc, char **argv, ListOptions *opt)
{
    char    **status   = clag_str ("status",   's', NULL,  "Filter by status");
    char    **priority = clag_str ("priority", 'p', NULL,  "Filter by priority");
    char    **tag      = clag_str ("tag",      'T', NULL,  "Filter by tag");
    bool     *all      = clag_bool("all",      'a', false, "Include closed issues");
    bool     *nohdr    = clag_bool("no-header",'q', false, "Suppress column header");
    uint64_t *limit    = clag_uint64("limit",  'n', 0,     "Max issues to show (0 = all)");

    clag_usage("[options]");

    if (!clag_parse(argc, argv)) {
        clag_print_error(stderr);
        return false;
    }

    opt->filter.status         = *status;
    opt->filter.priority       = *priority;
    opt->filter.tag            = *tag;
    opt->filter.include_closed = *all;

    opt->show_header = !*nohdr;
    opt->limit       = *limit;

    return true;
}

int cmd_list(int argc, char **argv)
{
    ListOptions opt = {0};
    if (!list__parse_opts(argc, argv, &opt)) return 1;

    if (!require_repo()) return 1;

    int result = 1;
    Temp_Checkpoint tmark = temp_save(); 
    File_Paths ids = {0};
    if (!fs_read_dir(".tatr/issues", &ids)) {
        log_error("cannot read issues directory");
        goto defer;
    }

    da_sort(&ids, cmp_paths);

    if (opt.show_header) ui_print_list_header();

    uint64_t shown = 0;
    for (size_t i = 0; i < ids.count; i++) {
        Issue iss;
        if (!issue_load(ids.items[i], &iss)) continue;

        if (!list__match(&iss, &opt.filter)) {
            issue_free(&iss);
            continue;
        }

        ui_print_issue_row(&iss);
        issue_free(&iss);

        shown++;
        if (opt.limit > 0 && shown >= opt.limit) break;
    }

    if (shown == 0) log_msg("(no issues)");

    result = 0;
defer:
    da_free(ids);
    temp_rewind(tmark);
    return result;
}
