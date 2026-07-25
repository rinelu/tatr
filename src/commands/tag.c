#include "cmd.h"
#include "tatr.h"

int cmd_tag(int argc, char **argv)
{
    bool      *remove = clag_bool("remove", 'r', false, "Remove the tag instead of adding");
    Clag_List *tags   = clag_list("tag",    't', ',',   "Tag(s) to add or remove");
    clag_usage("<id> [--tag <tag>,...] [--remove]");
    clag_required("tag");

    if (!clag_parse(argc, argv)) {
        clag_print_error(stderr);
        return 1;
    }
    if (!require_repo()) return 1;

    if (clag_rest_argc() < 1) {
        log_error("Missing issue ID");
        return 1;
    }

    const char *id = clag_rest_argv()[0];
    Temp_Checkpoint tmark = temp_save();
    const Renderer *r = renderer_for(TATR_FMT_HUMAN);

    Config cfg = {0};
    config_load(&cfg);
    const char *author = config_get(&cfg, "author");
    if (!author) author = USERNAME_ENV;
    config_free(&cfg);

    Tatr_Tag_Request req = {
        .tags   = (const char *const *)tags->items,
        .count  = tags->count,
        .remove = *remove,
    };

    const char *conflict = NULL;
    Tatr_Error err = tatr_issue_tag(id, &req, author, &conflict);

    if (err == TATR_ERR_NOT_FOUND) {
        log_error("Issue '%s' not found", id);
        temp_rewind(tmark);
        return 1;
    }
    if (err == TATR_ERR_CONFLICT) {
        if (*remove) log_warn("Tag '%s' not present on issue %s", conflict, id);
        else         log_warn("Tag '%s' already present on issue %s", conflict, id);
        temp_rewind(tmark);
        return 1;
    }
    if (err != TATR_OK) {
        log_error("Failed to save issue %s", id);
        temp_rewind(tmark);
        return 1;
    }

    r->message(stdout, temp_sprintf("Updated tags for issue %s", id));

    temp_rewind(tmark);
    return 0;
}
