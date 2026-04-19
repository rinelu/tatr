#include "astring.h"
#include "cmd.h"
#include "editor.h"
#include "temp.h"

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
    if (ev.count == 0 || sv_eq(ev, iv)) {
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
    char     **title      = clag_str ("title",       't', NULL,     "Issue title");
    char     **priority   = clag_str ("priority",    'p', "normal", "Priority");
    char     **status     = clag_str ("status",      's', "open",   "Status");
    Clag_List *tags       = clag_list("tag",         'T', ',',      "Tags");
    char     **body       = clag_str ("body",        'b', NULL,     "Body");
    char     **file       = clag_str ("file",        'F', NULL,     "Body from file");
    bool     *interactive = clag_bool("interactive", 'i', false,    "Interactive mode");

    clag_usage("[options]");
    clag_choices("status", "open", "closed", "wontfix", "in-progress");
    clag_choices("priority", "low", "normal", "high", "critical");
    
    if (!clag_parse(argc, argv)) {
        clag_print_error(stderr);
        return 1;
    }
    
    if (!require_repo()) return 1;

    int result = 1;

    String_Builder id = {0};
    generate_issue_id(&id);

    const char *issue_dir  = fs_path(".tatr/issues", id.items);
    const char *issue_file = fs_path(issue_dir, "issue.tatr");
    const char *attach_dir = fs_path(issue_dir, "attachments");

    Temp_Checkpoint tmark = temp_save();
    const char *path = fs_path(".tatr/issues", id.items);
    if (!fs_mkdir(path)) {
        log_error("Failed to create issue '%s'", id.items);
        goto defer;
    }

    fs_mkdir(fs_path(path, "attachments"));

    Issue iss;
    issue_init_empty(&iss);
    iss.id          = sv_from_cstr(id.items);
    iss.path        = issue_file;
    iss.dpath       = issue_dir;
    iss.attach_path = attach_dir;

    if (no_fields_provided(title, body, file, tags) && !*interactive) {
        if (open_full_editor_new(&iss)) goto defer;
        goto save_issue;
    }

    if (*title)    iss.title    = sv_from_cstr(*title);
    if (*status)   iss.status   = issue_status_from_cstr(*status);
    if (*priority) iss.priority = issue_priority_from_cstr(*priority);

    String_Builder tags_sb = {0};
    for (size_t i = 0; i < tags->count; i++) {
        if (i > 0) sb_append(&tags_sb, ',');
        sb_append_cstr(&tags_sb, tags->items[i]);
    }
    sb_append_null(&tags_sb);
    iss.tags = sv_from_cstr(tags_sb.items);

    char ts[64];
    timestamp_iso(ts, sizeof(ts));
    iss.created = sv_from_cstr(ts);

    String_Builder body_text = {0};
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

    String_Builder raw = {0};
    sb_appendf(&raw, "title: %s\n", iss.title.data ? iss.title.data : "");
    sb_appendf(&raw, "status: %s\n", issue_status_to_cstr(iss.status));
    sb_appendf(&raw, "priority: %s\n", issue_priority_to_cstr(iss.priority));
    sb_appendf(&raw, "created: %s\n", ts);
    sb_appendf(&raw, "tags: %s\n", tags_sb.items);
    sb_append_cstr(&raw, "---\n");

    if (body_text.count > 0) {
        sb_append_sv(&raw, sb_to_sv(body_text));
        if (body_text.items[body_text.count - 1] != '\n')
            sb_append_cstr(&raw, "\n");
    }

    iss.raw_sb = raw;

save_issue:
    if (!issue_save(&iss)) {
        log_error("failed to save issue");
        goto defer;
    }

    log_info("Created issue %s", id.items);
    tatrlog_append(TATRLOG_CREATE, id.items, *title ? *title : "");
    result = 0;

defer:
    if (result == 1) {
        fs_delete_recursive(path);
    }
    sb_free(id);
    sb_free(tags_sb);
    sb_free(body_text);
    issue_free(&iss);
    temp_rewind(tmark);
    return result;
}
