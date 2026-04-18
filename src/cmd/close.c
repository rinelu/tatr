#include "cmd.h"

static const char *reason_to_status(const char *reason)
{
    if (!reason) return "closed";
    if (strcmp(reason, "wontfix") == 0) return "wontfix";
    return "closed"; // default
}

int cmd_close(int argc, char **argv)
{
    char **reason = clag_str("reason", 'r', NULL, "Optional close reason: fixed | wontfix | duplicate");
    clag_usage("<id> [options]");

    if (!clag_parse(argc, argv)) {
        clag_print_error(stderr);
        return 1;
    }
    if (!require_repo()) return 1;

    if (clag_rest_argc() < 1) {
        log_error("tatr close: missing issue ID");
        return 1;
    }

    Temp_Checkpoint tmark = temp_save();
    int result = 1;
    const char *id = clag_rest_argv()[0];
    Issue iss;
    if (!issue_load(id, &iss)) {
        issue_free(&iss);
        goto defer;
    }

    const char *new_status = reason_to_status(*reason);

    bool ok = issue_replace_field(&iss, "status", new_status);
    if (!ok) {
        log_error("tatr: failed to close issue %s", id);
        goto defer;
    }
    tatrlog_append(TATRLOG_CLOSE, id, temp_sprintf("status=%s", new_status));
    log_info("Closed issue %s  (status: %s)", id, new_status);
    issue_save(&iss);

    result = 0;
defer:
    issue_free(&iss);
    temp_rewind(tmark);
    return result;
}
