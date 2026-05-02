#include "cmd.h"
#include "editor.h"
#include "config.h"

#include <stdio.h>
#include <string.h>

static void print_store(const Config_Store *s, const char *scope_label)
{
    if (s->entries.count == 0) return;

    printf("%s%s%s  %s(%s)%s\n",
           log_seq(A_BOLD), scope_label, log_seq(A_RESET),
           log_seq(A_DIM),  s->path,     log_seq(A_RESET));

    da_foreach(Config_Entry, e, &s->entries) {
        // Warn about unknown keys found on disk
        bool known = config_key_def(temp_strndup(e->key.data, e->key.count)) != NULL;
        printf("  %s"SV_Fmt"%s = %s"SV_Fmt"%s%s\n",
               log_seq(known ? A_BOLD_WHITE : A_YELLOW),
               SV_Arg(e->key), log_seq(A_RESET),
               log_seq(A_DIM), SV_Arg(e->val), log_seq(A_RESET),
               known ? "" : "  (unknown key)");
    }
    putchar('\n');
}

static void print_all_keys(void)
{
    printf("%sAvailable keys:%s\n\n", log_seq(A_BOLD), log_seq(A_RESET));
    for (size_t i = 0; i < CONFIG_KEYS_COUNT; i++) {
        const Config_Key_Def *d = &CONFIG_KEYS[i];
        printf("  %s%-20s%s %s\n",
               log_seq(A_BOLD_WHITE), d->key, log_seq(A_RESET),
               d->desc);
        if (d->default_val)
            printf("  %s%-20s%s default: %s\n",
                   log_seq(A_DIM), "", log_seq(A_RESET),
                   d->default_val);
    }
}

int cmd_config(int argc, char **argv)
{
    bool  *flag_local   = clag_bool("local",  0,  false, "Use local scope  (.tatr/config)");
    bool  *flag_global  = clag_bool("global", 0,  false, "Use global scope (~/.config/tatr/config)");
    bool  *flag_list    = clag_bool("list",  'l', false, "List all config values");
    bool  *flag_unset   = clag_bool("unset", 'u', false, "Remove a key");
    bool  *flag_edit    = clag_bool("edit",  'e', false, "Open config file in $EDITOR");
    bool  *flag_keys    = clag_bool("keys",  'k', false, "List all valid keys and their descriptions");

    clag_usage("[<key> [<value>]] [options]");
    clag_mutex("local", "global", NULL);

    if (!clag_parse(argc, argv)) {
        clag_print_error(stderr);
        return 1;
    }

    if (*flag_keys) {
        print_all_keys();
        return 0;
    }

    const char *scope_path = NULL;
    if (*flag_global)
        scope_path = config_global_path();
    else
        scope_path = CONFIG_LOCAL_PATH;

    if (*flag_list) {
        Config c = {0};
        config_load(&c);

        if (*flag_global) {
            print_store(&c.global, "global");
        } else if (*flag_local) {
            print_store(&c.local, "local");
        } else {
            // Show both, local first
            print_store(&c.local,  "local");
            print_store(&c.global, "global");

            // Show resolved view
            printf("%sresolved%s\n", log_seq(A_BOLD), log_seq(A_RESET));
            for (size_t i = 0; i < CONFIG_KEYS_COUNT; i++) {
                const char *key = CONFIG_KEYS[i].key;
                const char *val = config_get_or_default(&c, key);
                const char *src = store_get_source(&c, key);
                printf("  %s%-20s%s = %s  %s(%s)%s\n",
                       log_seq(A_BOLD_WHITE), key, log_seq(A_RESET),
                       val,
                       log_seq(A_DIM), src, log_seq(A_RESET));
            }
        }

        config_free(&c);
        return 0;
    }

    if (*flag_edit) {
        if (!fs_file_exists(scope_path)) {
            fs_mkdir_force(scope_path, true);

            String_Builder tmpl = {0};
            sb_append_cstr(&tmpl, "# tatr configuration\n");
            sb_append_cstr(&tmpl, "# Run `tatr config --keys` to see all valid keys.\n\n");
            for (size_t i = 0; i < CONFIG_KEYS_COUNT; i++) {
                sb_appendf(&tmpl, "# %s: %s\n",
                           CONFIG_KEYS[i].key, CONFIG_KEYS[i].desc);
                if (CONFIG_KEYS[i].default_val)
                    sb_appendf(&tmpl, "# default: %s\n",
                               CONFIG_KEYS[i].default_val);
                sb_append_char(&tmpl, '\n');
            }
            sb_append_null(&tmpl);
            fs_write_file(scope_path, tmpl.items, tmpl.count - 1);
            sb_free(tmpl);
        }

        if (!editor_open(scope_path)) {
            log_error("failed to open editor for '%s'", scope_path);
            return 1;
        }
        return 0;
    }

    int rest = clag_rest_argc();

    if (rest == 0) {
        clag_print_help(stdout);
        return 0;
    }

    const char *key = clag_rest_argv()[0];

    const Config_Key_Def *def = config_key_def(key);
    if (!def) {
        log_error("unknown config key '%s'", key);
        log_hint("run `tatr config --keys` to see valid keys");
        return 1;
    }

    if (*flag_unset) {
        if (!config_unset(scope_path, key)) {
            log_error("key '%s' not found in %s", key, scope_path);
            return 1;
        }
        log_info("unset %s in %s", key, scope_path);
        return 0;
    }

    if (rest == 1) {
        Config c = {0};
        config_load(&c);
        const char *val = config_get(&c, key);
        if (val) {
            printf("%s\n", val);
        } else {
            if (def->default_val)
                printf("%s%s%s  %s(default)%s\n",
                       log_seq(A_DIM), def->default_val, log_seq(A_RESET),
                       log_seq(A_DIM), log_seq(A_RESET));
            else {
                log_warn("'%s' is not set", key);
                config_free(&c);
                return 1;
            }
        }
        config_free(&c);
        return 0;
    }

    const char *val = clag_rest_argv()[1];

    if (!config_set(scope_path, key, val)) {
        log_error("failed to write to '%s'", scope_path);
        return 1;
    }

    log_info("set %s = %s  (%s)", key, val, scope_path);
    return 0;
}
