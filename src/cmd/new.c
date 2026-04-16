#include "cmd.h"

static void fill_random(unsigned char *buf, size_t len)
{
#if defined(__APPLE__) || defined(__FreeBSD__) || defined(__OpenBSD__) || defined(__NetBSD__)
    arc4random_buf(buf, len);

#elif defined(_WIN32)
#pragma comment(lib, "bcrypt.lib")
    BCryptGenRandom(NULL, buf, (ULONG)len, BCRYPT_USE_SYSTEM_PREFERRED_RNG);

#else
#include <sys/random.h>
#if defined(__linux__) && defined(__GLIBC__) && (__GLIBC__ > 2 || (__GLIBC__ == 2 && __GLIBC_MINOR__ >= 25))
    getrandom(buf, len, 0);
#else
    FILE *f = fopen("/dev/urandom", "rb");
    if (f) {
        fread(buf, 1, len, f);
        fclose(f);
        return;
    }
    for (size_t i = 0; i < len; i++)
        buf[i] = (unsigned char)(rand() ^ (int)(uintptr_t)(buf + i));
#endif
#endif
}

static void generate_issue_id(String_Builder *out)
{
    char time[32];
    timestamp_id(time, sizeof(time));

    unsigned char rnd[3];
    fill_random(rnd, sizeof(rnd));

    sb_appendf(out, "%s-%02x%02x%02x", time, rnd[0], rnd[1], rnd[2]);
    sb_append_null(out);
}

int cmd_new(int argc, char **argv)
{
    char     **title    = clag_str ("title",    't', NULL,     "Issue title");
    char     **priority = clag_str ("priority", 'p', "normal", "Priority: low | normal | high | critical");
    char     **status   = clag_str ("status",   's', "open",   "Initial status");
    Clag_List *tags     = clag_list("tag",      'T', ',',      "Comma-separated tags or repeat -T");
    char     **body     = clag_str ("body",     'b', NULL,     "Issue body text (optional, inline)");
    char     **file     = clag_str ("file",     'F', NULL,     "Read body from file ('-' for stdin)");

    clag_required("title");
    clag_usage("[options]  (creates a new issue)");

    if (!clag_parse(argc, argv)) {
        clag_print_error(stderr);
        clag_print_options(stderr);
        return 1;
    }

    const char *valid_priorities[] = { "low", "normal", "high", "critical", NULL };
    if (!str_in(*priority, valid_priorities)) {
        log_error("Invalid priority '%s'. Use: low | normal | high | critical", *priority);
        return 1;
    }

    const char *valid_statuses[] = { "open", "closed", "in-progress", "wontfix", NULL };
    if (!str_in(*status, valid_statuses)) {
        log_error("Invalid status '%s'. Use: open | closed | in-progress | wontfix", *status);
        return 1;
    }

    if (!require_repo()) return 1;

    Temp_Checkpoint tmark = temp_save();
    int result = 1;
    String_Builder id = {0};
    generate_issue_id(&id);

    const char *path = fs_path(".tatr/issues", id.items);
    if (!fs_mkdir(path)) {
        log_error("Failed to create issue directory '%s'", id.items);
        goto defer;
    }

    path = fs_path(".tatr/issues", id.items, "attachments");
    if (!fs_mkdir(path)) {
        log_error("Cannot create attachments directory for '%s'", id.items);
        goto defer;
    }

    String_Builder body_text = {0};
    if (*file) {
        if (!fs_read_file(*file, &body_text)) {
            log_error("Cannot read body from '%s'", *file);
            goto defer;
        }
    } else if (*body) {
        sb_append_cstr(&body_text, *body);
    }

    String_Builder tags_sb = {0};
    for (size_t i = 0; i < tags->count; i++) {
        if (i > 0) sb_append(&tags_sb, ',');
        sb_append_cstr(&tags_sb, tags->items[i]);
    }
    sb_append_null(&tags_sb);

    char ts[64];
    timestamp_iso(ts, sizeof(ts));

    String_Builder content = {0};
    sb_appendf(&content, "title: %s\n", *title);
    sb_appendf(&content, "status: %s\n", *status);
    sb_appendf(&content, "priority: %s\n", *priority);
    sb_appendf(&content, "created: %s\n", ts);
    sb_appendf(&content, "tags: %s\n", tags_sb.items);
    sb_append_cstr(&content, "---\n");

    if (body_text.count > 0) {
        sb_append_sv(&content, sb_to_sv(body_text));
        sb_append(&content, '\n');
    }

    sb_append_null(&content);

    path = fs_path(".tatr/issues", id.items, "issue.tatr");
    bool ok = fs_write_file(path, content.items, content.count - 1);
    if (!ok) {
        log_error("Cannot write issue file for '%s'", id.items);
        goto defer;
    }

    log_info("Created issue %s", id.items);
    result = 0;
defer:
    sb_free(id);
    sb_free(content);
    sb_free(tags_sb);
    sb_free(body_text);
    temp_rewind(tmark);
    return result;
}
