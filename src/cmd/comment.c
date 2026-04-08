#include "cmd.h"

int cmd_comment(int argc, char **argv)
{
    char **message = clag_str("message", 'm', NULL, "Comment text");
    char **author  = clag_str("author",  'a', NULL, "Author name/handle");

    clag_usage("<id> --message <text> [--author <name>]");
    clag_required("message");

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
    Issue iss;
    if (!issue_load(id, &iss)) {
        log_error("Issue '%s' not found", id);
        return 1;
    }

    size_t tmark = temp_save();
    bool result = 1;
    char ts[64];
    timestamp_iso(ts, sizeof(ts));

    sb_append_cstr(&iss.raw_sb, "\n---comment---\n");
    sb_appendf(&iss.raw_sb, "date: %s\n", ts);

    if (*author && **author)
        sb_appendf(&iss.raw_sb, "author: %s\n", *author);

    sb_append_cstr(&iss.raw_sb, "\n");
    sb_append_cstr(&iss.raw_sb, *message);
    sb_append_cstr(&iss.raw_sb, "\n");

    if (!issue_save(&iss)) {
        log_error("Cannot write comment to issue %s", id);
        goto defer;
    }

    log_info("Comment added to issue %s", id);
    result = 0;

defer:
    temp_rewind(tmark);
    issue_free(&iss);
    return result;
}
