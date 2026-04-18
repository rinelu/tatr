#include "cmd.h"

typedef struct {
    String_View *items;
    size_t count;
    size_t capacity;
} Tokens;

static bool match_token(String_View haystack, String_View token, bool case_flag)
{
    return case_flag ? sv_contains(haystack, token) : sv_icontains(haystack, token);
}

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

    int result = 1;
    Temp_Checkpoint tmark = temp_save();
    Tokens tokens = {0};
    for (int i = 0; i < clag_rest_argc(); i++)
        da_append(&tokens, sv_from_cstr(clag_rest_argv()[i]));

    File_Paths issues = {0};
    if (!fs_read_dir(".tatr/issues", &issues)) {
        log_error("Cannot read issues directory");
        goto defer;
    }

    da_sort(&issues, cmp_paths);

    size_t found = 0;
    for (size_t i = 0; i < issues.count; i++) {
        Issue iss = {0};
        if (!issue_load(issues.items[i], &iss)) continue;

        bool match = true;

        for (size_t t = 0; t < tokens.count; t++) {
            String_View tok = tokens.items[t];

            // field filter: key:value
            String_View rest = tok;
            String_View key  = sv_slice_by_delim(&rest, ':');

            if (rest.count > 0) {
                String_View value = rest;

                if (sv_eq_cstr(key, "tag") || sv_eq_cstr(key, "tags")) {
                    if (!sv_has(iss.tags, value.data, ','))
                        goto false_match;
                } else if (sv_eq_cstr(key, "status")) {
                    if (!match_token(iss.status, value, *case_flag))
                        goto false_match;
                } else if (sv_eq_cstr(key, "priority")) {
                    if (!match_token(iss.priority, value, *case_flag))
                        goto false_match;
                } else if (sv_eq_cstr(key, "title")) {
                    if (!match_token(iss.title, value, *case_flag))
                        goto false_match;
                } else {
                    // unknown field, fallback to global search
                    String_View hay = *header_only ? iss.header : iss.raw;
                    if (!match_token(hay, tok, *case_flag))
                        goto false_match;
                }
                continue;
            }

            // normal token
            String_View hay = *header_only ? iss.header : iss.raw;
            if (!match_token(hay, tok, *case_flag)) {
false_match:
                match = false;
                break;
            }
        }

        if (!match) {
            issue_free(&iss);
            continue;
        }

        ui_print_search_row(&iss);
        issue_free(&iss);

        found++;
        if (*limit > 0 && found >= *limit) break;
    }

    if (found == 0) {
        log_msg("(no results)");
        goto defer;
    }
    log_msg("(%zu result%s for '%s')", found, found == 1 ? "" : "s", clag_rest_argv()[0]);

    result = 0;
defer:
    da_free(issues);
    da_free(tokens);
    temp_rewind(tmark);
    return result;
}
