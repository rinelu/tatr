#include "cmd.h"
#include "issue.h"
#include "temp.h"
#include "ui.h"

typedef struct {
    size_t total;
    size_t open;
    size_t in_progress;
    size_t closed;
} Counts;

typedef struct {
    const char *id;
    String_View title;
    Issue_StatusKind status;
    Issue_PriorityKind priority;
    String_View tags;
    String_View created;
    String_View updated;
} IssueInfo;

typedef struct {
    IssueInfo *items;
    size_t count;
    size_t capacity;
} Issues;

static bool ii_is_closed(const IssueInfo *iss)
{
    return iss->status == ISSUE_SCLOSED ||
           iss->status == ISSUE_SWONTFIX;
}

// extract latest comment date if exists
static String_View issue_last_updated(const Issue *iss)
{
    String_View body = iss->body;
    String_View last = iss->created;

    while (body.count > 0) {
        String_View line = sv_trim(sv_slice_by_delim(&body, '\n'));

        if (sv_starts_with(line, sv_from_cstr("date:"))) {
            String_View rest = line;
            sv_slice_by_delim(&rest, ':');
            last = sv_trim(rest);
        }
    }

    return last;
}

#define parse_iso(x) parse_time_n(x.data, x.count)

static int cmp_recent(const void *a, const void *b)
{
    const IssueInfo *ia = a;
    const IssueInfo *ib = b;

    time_t ta = parse_iso(ia->updated);
    time_t tb = parse_iso(ib->updated);

    return (tb > ta) - (tb < ta);
}

typedef struct {
    String_View tag;
    size_t count;
} TagCount;

typedef struct {
    TagCount *items;
    size_t count;
    size_t capacity;
} Tags;

static int tag_find(Tags *tags, String_View t)
{
    for (size_t i = 0; i < tags->count; i++)
        if (sv_eq(tags->items[i].tag, t))
            return (int)i;
    return -1;
}

static int cmp_tag_count(const void *a, const void *b)
{
    const TagCount *ta = a;
    const TagCount *tb = b;
    return (tb->count > ta->count) - (tb->count < ta->count);
}

int cmd_status(int argc, char **argv)
{
    uint64_t *limit = clag_uint64("limit", 'n', 5, "Recent issues limit");
    uint64_t *stale_days = clag_uint64("stale-days", 0, 7, "Days before issue is stale");

    clag_usage("[options]");

    if (!clag_parse(argc, argv)) {
        clag_print_error(stderr);
        return 1;
    }
    if (!require_repo()) return 1;


    File_Paths ids = {0};
    if (!fs_read_dir(".tatr/issues", &ids)) {
        log_error("cannot read issues directory");
        da_free(ids);
        return 1;
    }

    Temp_Checkpoint tmark = temp_save();
    Counts c = {0};
    Issues issues = {0};
    Tags tags = {0};
    time_t now = time(NULL);

    for (size_t i = 0; i < ids.count; i++) {
        Issue iss;
        if (!issue_load(ids.items[i], &iss)) continue;

        c.total++;

        if (iss.status == ISSUE_SOPEN) c.open++;
        else if (iss.status == ISSUE_SONGOING) c.in_progress++;
        else if (issue_is_closed(&iss)) c.closed++;

#define TV(sv) sv_from_parts(temp_strndup((sv).data, (sv).count), (sv).count)
        IssueInfo info = {
            .id       = temp_strdup(ids.items[i]),
            .title    = TV(iss.title),
            .status   = iss.status,
            .priority = iss.priority,
            .tags     = TV(iss.tags),
            .created  = TV(iss.created),
            .updated  = TV(issue_last_updated(&iss)),
        };

        da_append(&issues, info);

        String_View tmp = iss.tags;
        while (tmp.count > 0) {
            String_View t = sv_trim(sv_slice_by_delim(&tmp, ','));
            if (t.count == 0) continue;

            int idx = tag_find(&tags, t);
            if (idx >= 0) {
                tags.items[idx].count++;
                continue;
            }

            TagCount tc = {
                .tag = TV(t),
                .count = 1
            };
            da_append(&tags, tc);
        }

        issue_free(&iss);
#undef TV
    }

    da_sort(&issues, cmp_recent);
    da_sort(&tags,   cmp_tag_count);

    log_msg("On .tatr/\n");
    log_msg("%zu issue%s  --  %s%zu open%s  %s%zu in-progress%s  %s%zu closed%s  %s(%.0f%% complete)%s",
        c.total, c.total == 1 ? "" : "s",
        A_GREEN, c.open, A_RESET,
        A_BLUE, c.in_progress, A_RESET,
        A_DIM, c.closed, A_RESET,
        A_DIM, c.total > 0 ? (double)c.closed / (double)c.total * 100.0 : 0.0, A_RESET);

    // High priority
    bool has_high = false;
    da_foreach(IssueInfo, iss, &issues) {
        if (ii_is_closed(iss)) continue;
        bool is_crit = iss->priority == ISSUE_PCRITICAL;
        if (!is_crit && iss->priority != ISSUE_PHIGH)
            continue;
        if (!has_high) { 
            log_msg("\n%sHigh priority:%s", A_BOLD_RED, A_RESET);
            has_high = true;
        }
        const char *pcol = is_crit ? A_BOLD_RED : A_YELLOW;
        log_msg("  %s%s%s  "SV_Fmt, pcol, iss->id, A_RESET, SV_Arg(iss->title));
    }
    if (!has_high) log_msg("\n%sHigh priority:%s %s(none)%s",
            A_BOLD, A_RESET, A_DIM, A_RESET);

    // Stale
    bool has_stale = false;
    da_foreach(IssueInfo, iss, &issues) {
        if (ii_is_closed(iss)) continue;
        double days = difftime(now, parse_iso(iss->updated)) / 86400.0;
        char time_rel[32];
        ui_format_time_relative(parse_iso(iss->updated), time_rel, sizeof(time_rel));
        if (days < (double)*stale_days)
            continue;

        if (!has_stale) {
            log_msg("\n%sStale%s  %s(no activity >%"PRIu64" days):%s",
                    A_BOLD_YELLOW, A_RESET, A_DIM, *stale_days, A_RESET);
            has_stale = true;
        }
        log_msg("  %s%s%s  "SV_Fmt"  %s(%s)%s", 
                A_YELLOW, iss->id, A_RESET,
                SV_Arg(iss->title), 
                A_DIM, time_rel, A_RESET);
    }
    if (!has_stale) log_msg("\n%sStale:%s %s(none)%s", A_BOLD, A_RESET, A_DIM, A_RESET);

    log_msg("\n%sRecent activity:%s", A_BOLD, A_RESET);
    if (issues.count == 0) {
        log_msg("  %s(no issues)%s", A_DIM, A_RESET);
    } else {
        for (size_t i = 0; i < issues.count && i < *limit; i++) {
            IssueInfo *iss = &issues.items[i];
            char time_rel[32];
            ui_format_time_relative(parse_iso(iss->updated), time_rel, sizeof(time_rel));
            log_msg("  %s%s%s  "SV_Fmt"  %s%s%s",
                   A_DIM, iss->id, A_RESET,
                   SV_Arg(iss->title),
                   A_DIM, time_rel, A_RESET);
        }
    }

    log_msg("\n%sTop tags:%s", A_BOLD, A_RESET);
    if (tags.count == 0) {
        log_msg("%s  (none)%s", A_DIM, A_RESET);
    } else {
        String_Builder row = {0};
        for (size_t i = 0; i < tags.count && i < 5; i++) {
            if (i > 0) sb_append_cstr(&row, "    ");
            sb_appendf(&row, "%s"SV_Fmt"%s  %sx%zu%s",
                    A_CYAN, SV_Arg(tags.items[i].tag), A_RESET,
                    A_DIM, tags.items[i].count, A_RESET);
        }
        sb_append_null(&row);
        log_msg("  %s", row.items);
        sb_free(row);
    }

    printf("\n");

    da_free(ids);
    da_free(issues);
    da_free(tags);
    temp_rewind(tmark);

    return 0;
}

#undef parse_iso
