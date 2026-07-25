#include "astring.h"
#include "cmd.h"
#include "config.h"
#include "editor.h"
#include "global.h"
#include "temp.h"
#include "tatr.h"

#if defined(__linux__)
#include <sys/random.h>
#include <errno.h>
#elif defined(_WIN32)
#include <windows.h>
#include <bcrypt.h>
#endif

// TODO: git-like UUID
static void fill_random(unsigned char *buf, size_t len)
{
#if defined(__APPLE__) || defined(__FreeBSD__) || defined(__OpenBSD__) || defined(__NetBSD__)
    arc4random_buf(buf, len);
#elif defined(_WIN32)
    LONG status = BCryptGenRandom(NULL, buf, (ULONG)len, BCRYPT_USE_SYSTEM_PREFERRED_RNG);
    if (status != 0 /* STATUS_SUCCESS */) {
        for (size_t i = 0; i < len; i++)
            buf[i] = (unsigned char)(rand() ^ (int)(uintptr_t)(buf + i));
    }
#else
#if defined(__linux__) && defined(__GLIBC__) && (__GLIBC__ > 2 || (__GLIBC__ == 2 && __GLIBC_MINOR__ >= 25))
    size_t filled = 0;
    while (filled < len) {
        ssize_t n = getrandom(buf + filled, len - filled, 0);
        if (n < 0) {
            if (errno == EINTR)
                continue;
            break;
        }
        filled += (size_t)n;
    }
    if (filled < len) {
        for (size_t i = filled; i < len; i++)
            buf[i] = (unsigned char)(rand() ^ (int)(uintptr_t)(buf + i));
    }
#else
    FILE *f = fopen("/dev/urandom", "rb");
    if (f) {
        size_t n = fread(buf, 1, len, f);
        fclose(f);
        for (size_t i = n; i < len; i++)
            buf[i] = (unsigned char)(rand() ^ (int)(uintptr_t)(buf + i));
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

static bool no_fields_provided(char **title, char **body, char **file, Clag_List *tags)
{
    return (!*title && !*body && !*file && tags->count == 0);
}

static int open_full_editor_new(Issue *iss)
{
    int result = 1;
    String_Builder initial = {0};
    sb_append_cstr(&initial,
        "title: \n"
        "status: open\n"
        "priority: normal\n"
        "tags: \n"
        "---\n\n");

    String_Builder edited = {0};
    bool ok = editor_edit(initial.items, initial.count, ".tatr", &edited);
    if (!ok) goto defer;

    String_View ev = sv_trim(sb_to_sv(edited));
    String_View iv = sv_trim(sb_to_sv(initial));
    if (sv_empty(ev) || sv_eq(ev, iv)) {
        log_warn("aborted (no changes)");
        goto defer;
    }

    iss->raw_sb = edited;
    result = 0;
defer:
    if (result == 1) sb_free(edited);
    sb_free(initial);
    return result;
}

static int maybe_edit_body(Issue *iss, String_Builder *body_text)
{
    (void)iss;
    if (body_text->count > 0) return 0;

    String_Builder edited = {0};
    if (!editor_edit("", 0, ".md", &edited)) {
        sb_free(edited);
        return 1;
    }

    if (edited.count > 0) {
        *body_text = edited;
        return 0;
    }

    sb_free(edited);
    return 0;
}

int cmd_new(int argc, char **argv)
{
    Config cfg = {0};
    config_load(&cfg);
    const char *def_priority = config_get_or_default(&cfg, "default_priority");
    const char *def_status   = config_get_or_default(&cfg, "default_status");
    const char *author       = config_get(&cfg, "author");
    if (!author) author = USERNAME_ENV;
    config_free(&cfg);

    char     **title      = clag_str ("title",       't', NULL,         "Issue title");
    char     **priority   = clag_str ("priority",    'p', def_priority, "Priority");
    char     **status     = clag_str ("status",      's', def_status,   "Status");
    Clag_List *tags       = clag_list("tag",         'T', ',',          "Tags");
    char     **body       = clag_str ("body",        'b', NULL,         "Body");
    char     **file       = clag_str ("file",        'F', NULL,         "Body from file");
    bool     *interactive = clag_bool("interactive", 'i', false,        "Interactive mode");

    clag_usage("[options]");
    clag_choices("status", "open", "closed", "wontfix", "in-progress");
    clag_choices("priority", "low", "normal", "high", "critical");

    if (!clag_parse(argc, argv)) {
        clag_print_error(stderr);
        return 1;
    }

    if (!require_repo()) return 1;

    int result = 1;
    String_Builder id        = {0};
    String_Builder tags_sb   = {0};
    String_Builder body_text = {0};
    generate_issue_id(&id);

    Temp_Checkpoint tmark = temp_save();
    const Renderer *r = renderer_for(TATR_FMT_HUMAN);

    Issue iss;
    if (!issue_create(id.items, &iss)) {
        log_error("Failed to create issue '%s'", id.items);
        issue_init_empty(&iss);
        goto defer;
    }

    if (no_fields_provided(title, body, file, tags) && !*interactive) {
        if (open_full_editor_new(&iss)) goto defer;

        if (tatr_issue_new_from_editor(&iss, id.items, author) != TATR_OK) {
            log_error("failed to save issue");
            goto defer;
        }

        r->message(stdout, temp_sprintf("Created issue %s", id.items));
        result = 0;
        goto defer;
    }

    for (size_t i = 0; i < tags->count; i++) {
        if (i > 0) sb_append(&tags_sb, ',');
        sb_append_cstr(&tags_sb, tags->items[i]);
    }
    sb_append_null(&tags_sb);

    if (*file) {
        if (!fs_read_file(*file, &body_text)) {
            log_error("Cannot read '%s'", *file);
            goto defer;
        }
    } else if (*body) {
        sb_append_cstr(&body_text, *body);
    } else if (*interactive) {
        if (maybe_edit_body(&iss, &body_text)) goto defer;
    }

    {
        Tatr_Issue_New_Params params = {
            .id        = id.items,
            .title     = *title,
            .status    = *status,
            .priority  = *priority,
            .tags_csv  = tags_sb.items,
            .body      = sb_to_sv(body_text),
            .author    = author,
        };

        if (tatr_issue_new(&params, &iss) != TATR_OK) {
            log_error("failed to save issue");
            goto defer;
        }
    }

    r->message(stdout, temp_sprintf("Created issue %s", id.items));
    result = 0;

defer:
    if (result == 1 && issue_exists(id.items)) {
        issue_delete(id.items);
    }
    sb_free(id);
    sb_free(tags_sb);
    sb_free(body_text);
    issue_free(&iss);
    temp_rewind(tmark);
    return result;
}
