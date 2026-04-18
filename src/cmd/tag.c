#include "cmd.h"
#include "temp.h"

typedef struct {
    String_View *items;
    size_t count;
    size_t capacity;
} Tags;

int cmd_tag(int argc, char **argv)
{
    bool      *remove = clag_bool("remove", 'r', false, "Remove the tag instead of adding");
    Clag_List *tags   = clag_list("tag",    't', ',',   "Tag(s) to add or remove");
    clag_usage("<id> [--tag <tag>,...] [--remove]");
    clag_required("tag");

    if (!clag_parse(argc, argv)) {
        clag_print_error(stderr);
        clag_print_options(stderr);
        return 1;
    }

    if (clag_rest_argc() < 1) {
        log_error("Missing issue ID");
        return 1;
    }

    const char *id = clag_rest_argv()[0];

    int result = 1;
    Temp_Checkpoint tmark = temp_save();
    Issue iss;
    if (!issue_load(id, &iss)) {
        log_error("Issue '%s' not found", id);
        return 1;
    }

    Tags owned = {0};
    String_Builder sb = {0};
    sv_split_by_delim(iss.tags, ',', &owned);

    for (size_t ti = 0; ti < tags->count; ti++) {
        const char *t = tags->items[ti];

        int found = -1;
        for (size_t k = 0; k < owned.count; k++) {
            if (!sv_eq_cstr(owned.items[k], t)) continue;
            found = (int)k;
            break;
        }

        if (*remove) {
            if (found < 0) {
                log_warn("Tag '%s' not present on issue %s", t, id);
                goto defer;
            }
            da_remove_unordered(&owned, (size_t)found);
            continue;
        }

        if (found >= 0) {
            log_warn("Tag '%s' already present on issue %s", t, id);
            goto defer;
        }

        da_append(&owned, sv_from_cstr(t));
    }

    for (size_t i = 0; i < owned.count; i++) {
        if (i > 0) sb_append_cstr(&sb, ",");
        sb_append_sv(&sb, owned.items[i]);
    }
    sb_append_null(&sb);

    issue_replace_field(&iss, "tags", sb.items);
    if (!issue_save(&iss)) {
        log_error("Failed to save issue %s", id);
        goto defer;
    }

    log_info("Updated tags for issue %s", id);
    result = 0;

defer:
    sb_free(sb);
    da_free(owned);
    issue_free(&iss);
    temp_rewind(tmark);

    return result;
}
