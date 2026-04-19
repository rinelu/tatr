#include "cmd.h"

static void show_issue_full(const Issue *iss)
{
    log_msg("%sissue %s"SV_Fmt"%s", log_seq(A_BYELLOW), log_seq(A_BOLD), SV_Arg(iss->id), log_seq(A_RESET));

#define LABEL(s) printf("%s%-10s%s", log_seq(A_BOLD_WHITE), (s), log_seq(A_RESET))

    LABEL("Author:");
    log_msg("  "SV_Fmt, SV_Arg(iss->created));

    LABEL("Status:");
    log_msg("  %s%s%s",
        log_seq(ui_status_color(iss->status)),
        issue_status_to_cstr(iss->status),
        log_seq(A_RESET));

    LABEL("Priority:");
    log_msg("  %s%s%s",
            log_seq(ui_priority_color(iss->priority)),
            issue_priority_to_cstr(iss->priority),
            log_seq(A_RESET));

    if (iss->tags.count)
        LABEL("Tags:"),
        log_msg("  %s"SV_Fmt"%s", log_seq(A_CYAN), SV_Arg(iss->tags), log_seq(A_RESET));

#undef LABEL

    log_msg("\n    %s%.*s%s", log_seq(A_BOLD), SV_Arg(iss->title), log_seq(A_RESET));

    if (iss->body.count) {
        String_View cursor = iss->body, line;

        while (cursor.count > 0) {
            line = sv_slice_by_delim(&cursor, '\n');

            String_View trimmed = sv_trim(line);
            if (sv_eq(trimmed, sv_from_cstr("---comment---"))) {
                print_rule();
                continue;
            }

            log_msg("    "SV_Fmt, SV_Arg(line));
        }
    }

    fputc('\n', stdout);
}

int cmd_show(int argc, char **argv)
{
    bool *raw_flag = clag_bool("raw", 'r', false, "Print raw issue.tatr without decoration");
    clag_usage("<id> [options]");

    if (!clag_parse(argc, argv)) {
        clag_print_error(stderr);
        return 1;
    }
    if (!require_repo()) return 1;

    if (clag_rest_argc() < 1) {
        log_error("tatr show: missing issue ID");
        log_msg("usage: tatr show <id> [--raw]");
        return 1;
    }

    const char *id = clag_rest_argv()[0];

    int result = 1;
    Temp_Checkpoint tmark = temp_save();
    Issue iss;
    if (!issue_load(id, &iss)) {
        log_error("tatr: issue '%s' not found", id);
        goto defer;
    }

    result = 0;
    if (*raw_flag) {
        log_msg(SV_Fmt, SV_Arg(iss.raw));
        goto defer;
    }

    show_issue_full(&iss);

    File_Paths files = {0};
    if (fs_read_dir(iss.attach_path, &files)) {
        log_msg("\nAttachments (%zu):", files.count);
        for (size_t i = 0; i < files.count; i++)
            log_msg("  %s", files.items[i]);
        da_free(files);
    }

defer:
    issue_free(&iss);
    temp_rewind(tmark);
    return result;
}
