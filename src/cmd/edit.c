#include "cmd.h"

static const char *EDITABLE_FIELDS[] = {
    "title", "status", "priority", "tags", NULL
};

static String_View get_field(const Issue *iss, const char *field)
{
    if (strcmp(field, "title") == 0)    return iss->title;
    if (strcmp(field, "status") == 0)   return iss->status;
    if (strcmp(field, "priority") == 0) return iss->priority;
    if (strcmp(field, "tags") == 0)     return iss->tags;
    return sv_from_parts(NULL, 0);
}

int cmd_edit(int argc, char **argv)
{
    char **field = clag_str("field", 'f', NULL, "Field to edit: title | status | priority | tags");
    char **value = clag_str("value", 'v', NULL, "New value for the field");

    clag_usage("<id> --field <field> --value <value>");
    clag_required("field");
    clag_required("value");

    if (!clag_parse(argc, argv)) {
        clag_print_error(stderr);
        clag_print_options(stderr);
        return 1;
    }

    if (clag_rest_argc() < 1) {
        ui_error("missing issue ID");
        return 1;
    }

    const char *id = clag_rest_argv()[0];
    if (!str_in(*field, EDITABLE_FIELDS)) {
        ui_error("unknown field '%s' (use: title | status | priority | tags)", *field);
        return 1;
    }

    size_t tmark = temp_save();
    int result = 1;
    Issue iss;
    if (!issue_load(id, &iss)) {
        ui_error("issue '%s' not found", id);
        goto defer;
    }

    String_View old = get_field(&iss, *field);
    if (sv_eq(old, sv_from_cstr(*value))) {
        ui_warn("no change: '%s' is already '%s'", *field, *value);
        result = 0;
        goto defer;
    }

    if (!issue_replace_field(&iss, *field, *value)) {
        ui_error("failed to update '%s'", *field);
        goto defer;
    }

    ui_info("Updated %s:", *field);
    ui_msg("  - old: "SV_Fmt, SV_Arg(old));
    ui_msg("  - new: %s", *value);
    ui_msg("  issue: %s", id);

    issue_save(&iss);
    result = 0;
defer:
    issue_free(&iss);
    temp_rewind(tmark);
    return result;
}
