#include "cmd.h"
#include "editor.h"
#include "tatr.h"

#include <stdio.h>
#include <string.h>

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

    Temp_Checkpoint tmark = temp_save();
    const Renderer *r = renderer_for(TATR_FMT_HUMAN);

    if (*flag_keys) {
        Tatr_Config_Keys_Result result;
        tatr_config_keys(&result);
        r->config_keys(stdout, &result);
        temp_rewind(tmark);
        return 0;
    }

    const char *scope_path = NULL;
    if (*flag_global)
        scope_path = config_global_path();
    else
        scope_path = TATR_CONFIG_PATH;

    if (*flag_list) {
        Config c = {0};
        config_load(&c);

        Tatr_Config_List_Result result;
        tatr_config_list(&c, *flag_local, *flag_global, &result);
        r->config_list(stdout, &result);

        config_free(&c);
        temp_rewind(tmark);
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
            temp_rewind(tmark);
            return 1;
        }
        temp_rewind(tmark);
        return 0;
    }

    int rest = clag_rest_argc();

    if (rest == 0) {
        clag_print_help(stdout);
        temp_rewind(tmark);
        return 0;
    }

    const char *key = clag_rest_argv()[0];

    const Config_Key_Def *def = config_key_def(key);
    if (!def) {
        log_error("unknown config key '%s'", key);
        log_hint("run `tatr config --keys` to see valid keys");
        temp_rewind(tmark);
        return 1;
    }

    if (*flag_unset) {
        if (!config_unset(scope_path, key)) {
            log_error("key '%s' not found in %s", key, scope_path);
            temp_rewind(tmark);
            return 1;
        }
        r->message(stdout, temp_sprintf("unset %s in %s", key, scope_path));
        temp_rewind(tmark);
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
                temp_rewind(tmark);
                return 1;
            }
        }
        config_free(&c);
        temp_rewind(tmark);
        return 0;
    }

    const char *val = clag_rest_argv()[1];

    Tatr_Error err = config_set(scope_path, key, val);
    if (err == TATR_ERR_INVALID_ARG) {
        log_error("invalid value '%s' for '%s' (expected %s)",
                  val, key, config_expected_desc(def));
        temp_rewind(tmark);
        return 1;
    }
    if (err != TATR_OK) {
        log_error("failed to write to '%s'", scope_path);
        temp_rewind(tmark);
        return 1;
    }

    r->message(stdout, temp_sprintf("set %s = %s  (%s)", key, val, scope_path));
    temp_rewind(tmark);
    return 0;
}
