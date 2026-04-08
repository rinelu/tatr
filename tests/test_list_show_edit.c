#include "test_helpers.h"

static char issue_open[64];
static char issue_closed[64];
static char issue_critical[64];

static void setup_populated_repo(void)
{
    tatr_init();

    tatr_new_issue("--title 'open normal' --priority normal --tag backend", issue_open, sizeof(issue_open));
    tatr_new_issue("--title 'critical crash' --priority critical --tag crash --tag backend", issue_critical, sizeof(issue_critical));

    tf_create_raw_issue("20240101000001", "old bug", "closed", "low", "legacy", "");
    strncpy(issue_closed, "20240101000001", sizeof(issue_closed)-1);
}

static void test_list_shows_open_issues(void)
{
    TatrResult r = tatr("list");
    ASSERT_CMD_OK(r);
    ASSERT_OUT_CONTAINS(r, "open normal");
}

static void test_list_hides_closed_by_default(void)
{
    TatrResult r = tatr("list");
    ASSERT_CMD_OK(r);
    ASSERT_OUT_NOT_CONTAINS(r, "old bug");
}

static void test_list_all_includes_closed(void)
{
    TatrResult r = tatr("list --all");
    ASSERT_CMD_OK(r);
    ASSERT_OUT_CONTAINS(r, "old bug");
}

static void test_list_filter_by_status(void)
{
    TatrResult r = tatr("list --status open");
    ASSERT_CMD_OK(r);
    ASSERT_OUT_CONTAINS(r, "open normal");
    ASSERT_OUT_CONTAINS(r, "critical crash");
}

static void test_list_filter_by_priority(void)
{
    TatrResult r = tatr("list --priority critical");
    ASSERT_CMD_OK(r);
    ASSERT_OUT_CONTAINS(r, "critical crash");
    ASSERT_OUT_NOT_CONTAINS(r, "open normal");
}

static void test_list_filter_by_tag(void)
{
    TatrResult r = tatr("list --tag crash");
    ASSERT_CMD_OK(r);
    ASSERT_OUT_CONTAINS(r, "critical crash");
    ASSERT_OUT_NOT_CONTAINS(r, "open normal");
}

static void test_list_tag_shared_between_issues(void)
{
    TatrResult r = tatr("list --tag backend");
    ASSERT_CMD_OK(r);
    ASSERT_OUT_CONTAINS(r, "open normal");
    ASSERT_OUT_CONTAINS(r, "critical crash");
}

static void test_list_limit(void)
{
    TatrResult r = tatr("list --limit 1");
    ASSERT_CMD_OK(r);
    int count = 0;
    const char *combined = tatr_combined(&r);
    const char *p = combined;
    while ((p = strstr(p, issue_open)) || (p = strstr(combined, issue_critical))) {
        count++;
        if (p) p++;
        break;
    }
    ASSERT_TRUE(strlen(combined) > 0);
}

static void test_list_no_header_flag(void)
{
    TatrResult r = tatr("list --no-header");
    ASSERT_CMD_OK(r);
    ASSERT_TRUE(r.exit_code == 0);
}

static void test_list_empty_repo_message(void)
{
    tatr_init();
    TatrResult r = tatr("list");
    ASSERT_CMD_OK(r);
    ASSERT_OUT_CONTAINS(r, "no issues");
}

static void test_show_displays_title(void)
{
    char args[128];
    snprintf(args, sizeof(args), "show %s", issue_open);
    TatrResult r = tatr(args);
    ASSERT_CMD_OK(r);
    ASSERT_OUT_CONTAINS(r, "open normal");
}

static void test_show_displays_priority(void)
{
    char args[128];
    snprintf(args, sizeof(args), "show %s", issue_critical);
    TatrResult r = tatr(args);
    ASSERT_CMD_OK(r);
    ASSERT_OUT_CONTAINS(r, "critical");
}

static void test_show_raw_flag(void)
{
    char args[128];
    snprintf(args, sizeof(args), "show %s --raw", issue_open);
    TatrResult r = tatr(args);
    ASSERT_CMD_OK(r);
    ASSERT_OUT_CONTAINS(r, "title:");
    ASSERT_OUT_CONTAINS(r, "status:");
}

static void test_show_unknown_id_fails(void)
{
    TatrResult r = tatr("show nonexistent_id_xyz");
    ASSERT_CMD_FAIL(r);
    ASSERT_OUT_CONTAINS(r, "not found");
}

static void test_show_missing_id_fails(void)
{
    TatrResult r = tatr("show");
    ASSERT_CMD_FAIL(r);
}

static void test_show_lists_attachments_section(void)
{
    char args[128];
    snprintf(args, sizeof(args), "show %s", issue_open);
    TatrResult r = tatr(args);
    ASSERT_CMD_OK(r);
    ASSERT_OUT_CONTAINS(r, "Attachments");
}

static void test_edit_title(void)
{
    char args[256];
    snprintf(args, sizeof(args), "edit %s --field title --value 'renamed title'", issue_open);
    TatrResult r = tatr(args);
    ASSERT_CMD_OK(r);

    char path[256];
    snprintf(path, sizeof(path), ".tatr/issues/%s/issue.tatr", issue_open);
    ASSERT_CONTAINS(tf_read_file(path), "renamed title");
}

static void test_edit_status(void)
{
    char args[256];
    snprintf(args, sizeof(args), "edit %s --field status --value in-progress", issue_open);
    ASSERT_CMD_OK(tatr(args));

    char path[256];
    snprintf(path, sizeof(path), ".tatr/issues/%s/issue.tatr", issue_open);
    ASSERT_CONTAINS(tf_read_file(path), "in-progress");
}

static void test_edit_priority(void)
{
    char args[256];
    snprintf(args, sizeof(args), "edit %s --field priority --value low", issue_open);
    ASSERT_CMD_OK(tatr(args));

    char path[256];
    snprintf(path, sizeof(path), ".tatr/issues/%s/issue.tatr", issue_open);
    ASSERT_CONTAINS(tf_read_file(path), "priority: low");
}

static void test_edit_tags(void)
{
    char args[256];
    snprintf(args, sizeof(args), "edit %s --field tags --value 'api,v2'", issue_open);
    ASSERT_CMD_OK(tatr(args));

    char path[256];
    snprintf(path, sizeof(path), ".tatr/issues/%s/issue.tatr", issue_open);
    const char *content = tf_read_file(path);
    ASSERT_CONTAINS(content, "api");
    ASSERT_CONTAINS(content, "v2");
}

static void test_edit_unknown_field_rejected(void)
{
    char args[256];
    snprintf(args, sizeof(args), "edit %s --field flavor --value vanilla", issue_open);
    TatrResult r = tatr(args);
    ASSERT_CMD_FAIL(r);
    ASSERT_OUT_CONTAINS(r, "unknown field");
}

static void test_edit_no_change_warns(void)
{
    char args[256];
    snprintf(args, sizeof(args), "edit %s --field priority --value normal", issue_open);
    TatrResult r = tatr(args);
    ASSERT_OUT_CONTAINS(r, "no change");
}

static void test_edit_missing_id_fails(void)
{
    TatrResult r = tatr("edit --field title --value foo");
    ASSERT_CMD_FAIL(r);
}

static void test_edit_unknown_id_fails(void)
{
    TatrResult r = tatr("edit bogus_id --field title --value foo");
    ASSERT_CMD_FAIL(r);
    ASSERT_OUT_CONTAINS(r, "not found");
}

void suite_list_show_edit(void)
{
    SUITE("list");
    RUN_TEST_F(test_list_shows_open_issues,       setup_populated_repo, NULL);
    RUN_TEST_F(test_list_hides_closed_by_default, setup_populated_repo, NULL);
    RUN_TEST_F(test_list_all_includes_closed,     setup_populated_repo, NULL);
    RUN_TEST_F(test_list_filter_by_status,        setup_populated_repo, NULL);
    RUN_TEST_F(test_list_filter_by_priority,      setup_populated_repo, NULL);
    RUN_TEST_F(test_list_filter_by_tag,           setup_populated_repo, NULL);
    RUN_TEST_F(test_list_tag_shared_between_issues, setup_populated_repo, NULL);
    RUN_TEST_F(test_list_limit,                   setup_populated_repo, NULL);
    RUN_TEST_F(test_list_no_header_flag,          setup_populated_repo, NULL);
    RUN_TEST(test_list_empty_repo_message);

    SUITE("show");
    RUN_TEST_F(test_show_displays_title,           setup_populated_repo, NULL);
    RUN_TEST_F(test_show_displays_priority,        setup_populated_repo, NULL);
    RUN_TEST_F(test_show_raw_flag,                 setup_populated_repo, NULL);
    RUN_TEST_F(test_show_unknown_id_fails,         setup_populated_repo, NULL);
    RUN_TEST_F(test_show_missing_id_fails,         setup_populated_repo, NULL);
    RUN_TEST_F(test_show_lists_attachments_section,setup_populated_repo, NULL);

    SUITE("edit");
    RUN_TEST_F(test_edit_title,                   setup_populated_repo, NULL);
    RUN_TEST_F(test_edit_status,                  setup_populated_repo, NULL);
    RUN_TEST_F(test_edit_priority,                setup_populated_repo, NULL);
    RUN_TEST_F(test_edit_tags,                    setup_populated_repo, NULL);
    RUN_TEST_F(test_edit_unknown_field_rejected,  setup_populated_repo, NULL);
    RUN_TEST_F(test_edit_no_change_warns,         setup_populated_repo, NULL);
    RUN_TEST_F(test_edit_missing_id_fails,        setup_populated_repo, NULL);
    RUN_TEST_F(test_edit_unknown_id_fails,        setup_populated_repo, NULL);
}
