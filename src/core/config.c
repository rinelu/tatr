#include "config.h"
#include "astring.h"
#include "array.h"
#include "fs.h"
#include "global.h"
#include "temp.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#include <pwd.h>
#endif

static const char *const STATUS_ENUM_VALUES[]   = { "open", "closed", "wontfix", "in-progress", NULL };
static const char *const PRIORITY_ENUM_VALUES[] = { "low",  "normal", "high",    "critical",    NULL };

const Config_Key_Def CONFIG_KEYS[] = {
    { "author",           "Default author name for comments and log entries", "unknown", CFG_STRING, NULL                 },
    { "default_status",   "Default status for new issues",                    "open",    CFG_ENUM,   STATUS_ENUM_VALUES   },
    { "default_priority", "Default priority for new issues",                  "normal",  CFG_ENUM,   PRIORITY_ENUM_VALUES },
    { "default_editor",   "Editor to use instead of $VISUAL/$EDITOR",         NULL,      CFG_STRING, NULL                 },
    { "log.limit",        "Default --limit for `tatr log`",                   "0",       CFG_INT,    NULL                 },
    { "list.show_closed", "Show closed issues in `tatr list` by default",     "false",   CFG_BOOL,   NULL                 },
    { "list.limit",       "Default --limit for `tatr list`",                  "0",       CFG_INT,    NULL                 },
};

const size_t CONFIG_KEYS_COUNT = sizeof(CONFIG_KEYS) / sizeof(CONFIG_KEYS[0]);

const Config_Key_Def *config_key_def(const char *key)
{
    for (size_t i = 0; i < CONFIG_KEYS_COUNT; i++)
        if (strcmp(CONFIG_KEYS[i].key, key) == 0)
            return &CONFIG_KEYS[i];
    return NULL;
}

static bool is_valid_int(const char *val)
{
    if (!val || !*val) return false;

    const char *p = val;
    if (*p == '-' || *p == '+') p++;
    if (!*p) return false;

    for (; *p; p++)
        if (*p < '0' || *p > '9') return false;

    return true;
}

static bool is_valid_bool(const char *val)
{
    return val && (strcmp(val, "true") == 0 || strcmp(val, "false") == 0 ||
                   strcmp(val, "1")    == 0 || strcmp(val, "0")     == 0);
}

bool config_validate(const Config_Key_Def *def, const char *val)
{
    if (!def || !val) return false;

    switch (def->type) {
        case CFG_STRING: return true;
        case CFG_BOOL:   return is_valid_bool(val);
        case CFG_INT:    return is_valid_int(val);
        case CFG_ENUM:
            if (!def->enum_values) return true;
            for (const char *const *p = def->enum_values; *p; p++)
                if (strcmp(*p, val) == 0) return true;
            return false;
    }
    return false;
}

const char *config_expected_desc(const Config_Key_Def *def)
{
    if (!def) return "a valid value";

    switch (def->type) {
        case CFG_STRING: return "any string";
        case CFG_BOOL:   return "true or false";
        case CFG_INT:    return "an integer";
        case CFG_ENUM: {
            if (!def->enum_values) return "any string";
            static char buf[256];
            size_t n = 0;
            n += (size_t)snprintf(buf + n, sizeof(buf) - n, "one of: ");
            for (const char *const *p = def->enum_values; *p; p++) {
                n += (size_t)snprintf(buf + n, sizeof(buf) - n, "%s%s",
                    p == def->enum_values ? "" : ", ", *p);
                if (n >= sizeof(buf)) break;
            }
            return buf;
        }
    }
    return "a valid value";
}

const char *config_global_path(void)
{
#ifdef _WIN32
    const char *appdata = getenv("APPDATA");
    if (!appdata || !*appdata) appdata = "C:\\Users\\Default\\AppData\\Roaming";
    return temp_sprintf("%s\\tatr\\config", appdata);
#else
    // XDG_CONFIG_HOME takes priority
    const char *xdg = getenv("XDG_CONFIG_HOME");
    if (xdg && *xdg)
        return temp_sprintf("%s/tatr/config", xdg);

    const char *home = getenv("HOME");
    if (!home || !*home) {
        struct passwd *pw = getpwuid(getuid());
        if (pw) home = pw->pw_dir;
    }
    if (!home || !*home) home = "/tmp";
    return temp_sprintf("%s/.config/tatr/config", home);
#endif
}

static void store_load(Config_Store *s, const char *path)
{
    memset(s, 0, sizeof(*s));
    s->path = strdup(path);

    String_Builder raw = {0};
    if (!fs_read_file(path, &raw)) {
        sb_free(raw);
        return;
    }
    sb_append_null(&raw);

    s->_buf = raw.items;

    String_View cursor = sv_from_parts(s->_buf, raw.count - 1);
    while (!sv_empty(cursor)) {
        String_View line = sv_trim_right(sv_slice_by_delim(&cursor, '\n'));
        line = sv_trim(line);

        if (sv_empty(line) || line.data[0] == '#') continue;

        String_View rest = line;
        String_View key  = sv_trim(sv_slice_by_delim(&rest, ':'));
        String_View val  = sv_trim(rest);

        if (sv_empty(key)) continue;

        Config_Entry e = { key, val };
        da_append(&s->entries, e);
    }
}

static void store_free(Config_Store *s)
{
    da_free(s->entries);
    free(s->_buf);
    free(s->path);
    memset(s, 0, sizeof(*s));
}

void config_load(Config *c)
{
    memset(c, 0, sizeof(*c));
    store_load(&c->local,  TATR_CONFIG_PATH);
    store_load(&c->global, config_global_path());
}

void config_free(Config *c)
{
    store_free(&c->local);
    store_free(&c->global);
}

static const char *store_get(const Config_Store *s, const char *key)
{
    for (size_t i = 0; i < s->entries.count; i++)
        if (sv_eq_cstr(s->entries.items[i].key, key))
            return temp_strndup(s->entries.items[i].val.data, s->entries.items[i].val.count);
    return NULL;
}

const char *store_get_source(const Config *c, const char *key)
{
    if (store_get(&c->local, key))  return "local";
    if (store_get(&c->global, key)) return "global";
    return "default";
}

const char *config_get(const Config *c, const char *key)
{
    const char *v = store_get(&c->local, key);
    if (v) return v;
    return store_get(&c->global, key);
}

const char *config_get_or_default(const Config *c, const char *key)
{
    const char *v = config_get(c, key);
    if (v) return v;
    const Config_Key_Def *def = config_key_def(key);
    if (def && def->default_val) return def->default_val;
    return "";
}

bool config_get_bool(const Config *c, const char *key)
{
    return parse_bool(config_get_or_default(c, key));
}

int64_t config_get_int(const Config *c, const char *key)
{
    const char *val = config_get_or_default(c, key);
    if (!is_valid_int(val)) return 0;
    return strtoll(val, NULL, 10);
}

Tatr_Error config_set(const char *path, const char *key, const char *val)
{
    const Config_Key_Def *def = config_key_def(key);
    if (!def) return TATR_ERR_INVALID_ARG;
    if (!config_validate(def, val)) return TATR_ERR_INVALID_ARG;

    {
        char *p = strdup(path);
        char *slash = strrchr(p, '/');
#ifdef _WIN32
        char *bslash = strrchr(p, '\\');
        if (bslash > slash) slash = bslash;
#endif
        if (slash && slash != p) {
            *slash = '\0';
            fs_mkdir_force(p, true);
        }
        free(p);
    }

    String_Builder raw = {0};
    bool existed = fs_read_file(path, &raw);
    if (existed) sb_append_null(&raw);

    String_Builder out = {0};
    bool done = false;
    if (existed) {
        String_View cursor = sv_from_parts(raw.items, raw.count - 1);

        while (!sv_empty(cursor)) {
            String_View line = sv_trim_right(sv_slice_by_delim(&cursor, '\n'));

            // Preserve comments and blank lines verbatim
            if (sv_empty(sv_trim(line)) || sv_trim(line).data[0] == '#') {
                sb_append_sv(&out, line);
                sb_append_char(&out, '\n');
                continue;
            }

            String_View rest = line;
            String_View k    = sv_trim(sv_slice_by_delim(&rest, ':'));

            if (sv_eq_cstr(k, key)) {
                sb_appendf(&out, "%s: %s\n", key, val);
                done = true;
            } else {
                sb_append_sv(&out, line);
                sb_append_char(&out, '\n');
            }
        }
    }

    if (!done) sb_appendf(&out, "%s: %s\n", key, val);

    sb_append_null(&out);
    bool ok = fs_write_file(path, out.items, out.count - 1);

    sb_free(raw);
    sb_free(out);
    return ok ? TATR_OK : TATR_ERR_IO;
}

bool config_unset(const char *path, const char *key)
{
    String_Builder raw = {0};
    if (!fs_read_file(path, &raw)) {
        sb_free(raw);
        return false;
    }
    sb_append_null(&raw);

    String_Builder out = {0};
    bool found = false;

    String_View cursor = sv_from_parts(raw.items, raw.count - 1);
    while (!sv_empty(cursor)) {
        String_View line = sv_trim_right(sv_slice_by_delim(&cursor, '\n'));

        if (!sv_empty(sv_trim(line)) && sv_trim(line).data[0] != '#') {
            String_View rest = line;
            String_View k    = sv_trim(sv_slice_by_delim(&rest, ':'));

            if (sv_eq_cstr(k, key)) {
                found = true;
                continue;
            }
        }

        sb_append_sv(&out, line);
        sb_append_char(&out, '\n');
    }

    bool ok = false;
    if (found) {
        sb_append_null(&out);
        ok = fs_write_file(path, out.items, out.count - 1);
    }

    sb_free(raw);
    sb_free(out);
    return ok;
}

bool config_write_global_default(void)
{
    const char *path   = config_global_path();
    String_Builder tmp = {0};
    if (fs_read_file(path, &tmp)) {
        sb_free(tmp);
        return true;
    }
    sb_free(tmp);

    // Ensure parent directory exists
    {
        char *p = strdup(path);
        if (!p) return false;

        char *slash = strrchr(p, '/');
#ifdef _WIN32
        char *bslash = strrchr(p, '\\');
        if (bslash > slash) slash = bslash;
#endif

        if (slash && slash != p) {
            *slash = '\0';
            fs_mkdir_force(p, true);
        }

        free(p);
    }

    // Build default config
    String_Builder out = {0};
    for (size_t i = 0; i < CONFIG_KEYS_COUNT; i++) {
        const Config_Key_Def *k = &CONFIG_KEYS[i];

        if (k->default_val)
            sb_appendf(&out, "%s: %s\n", k->key, k->default_val);
        else
            sb_appendf(&out, "# %s: %s\n", k->key, k->desc);
    }

    sb_append_null(&out);

    bool ok = fs_write_file(path, out.items, out.count - 1);
    sb_free(out);

    return ok;
}
