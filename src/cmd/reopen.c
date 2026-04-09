#include "cmd.h"

int cmd_reopen(int argc, char **argv)
{
    clag_usage("<id>");

    if (!clag_parse(argc, argv)) {
        clag_print_error(stderr);
        return 1;
    }

    if (clag_rest_argc() < 1) {
        log_error("tatr reopen: missing issue ID");
        return 1;
    }

    const char *id = clag_rest_argv()[0];
    int result = 1;
    size_t tmark = temp_save();

    Issue iss;
    if (!issue_load(id, &iss)) {
        log_error("issue '%s' not found", id);
        goto defer;
    }

    if (!issue_replace_field(&iss, "status", "open")) {
        log_error("tatr: failed to reopen issue");
        goto defer;
    }

    log_info("Reopened issue %s", id);
    result = 0;
    issue_save(&iss);

defer:
    temp_rewind(tmark);
    issue_free(&iss);
    return result;
}
