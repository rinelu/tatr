#include "astring.h"
#include "cmd.h"

int cmd_comment(int argc, char **argv)
{
    char **message = clag_str("message", 'm', NULL, "Comment text");

    clag_usage("<id> --message <text> [--author <name>]");
    clag_required("message");

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
    Issue iss;
    if (!issue_load(id, &iss)) {
        log_error("Issue '%s' not found", id);
        return 1;
    }

    Config cfg = {0};
    config_load(&cfg);
    const char *author  = config_get(&cfg, "author");
    if (!author) author = getenv("USER");
    config_free(&cfg);

    bool result = 1;
    char ts[64];
    timestamp_iso(ts, sizeof(ts));

    sb_append_cstr(&iss.raw_sb, "\n---comment---\n");
    sb_appendf(&iss.raw_sb, "date: %s\n", ts);
    sb_appendf(&iss.raw_sb, "author: %s\n", author);

    sb_append_cstr(&iss.raw_sb, "\n");
    sb_append_cstr(&iss.raw_sb, *message);
    sb_append_cstr(&iss.raw_sb, "\n");

    if (!issue_save(&iss)) {
        log_error("Cannot write comment to issue %s", id);
        goto defer;
    }

    TLOG(TATRLOG_COMMENT, id, {
        tatrlog_field(&__log, "author", author);
        tatrlog_body(&__log, temp_sv_to_cstr(iss.raw));
    });

    log_info("Comment added to issue %s", id);
    result = 0;

defer:
    temp_rewind(tmark);
    issue_free(&iss);
    return result;
}
