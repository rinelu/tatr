#include "test_helpers.h"

static void setup_mixed_repo(void)
{
    tatr_init();

    char id[64];
    tatr_new_issue("--title 'open one'   --priority normal --tag backend", id, sizeof(id));
    tatr_new_issue("--title 'open two'   --priority high   --tag frontend", id, sizeof(id));
    tatr_new_issue("--title 'in progress' --status in-progress --priority critical --tag backend", id, sizeof(id));

    tf_create_raw_issue("20230101000001", "old closed", "closed", "low", "legacy", "");
}

static void test_status_shows_total_count(void)
{
    TatrResult r = tatr("status");
    ASSERT_CMD_OK(r);
    ASSERT_OUT_CONTAINS(r, "issue");
}

static void test_status_shows_open_count(void)
{
    TatrResult r = tatr("status");
    ASSERT_CMD_OK(r);
    ASSERT_OUT_CONTAINS(r, "open");
}

static void test_status_shows_closed_section(void)
{
    TatrResult r = tatr("status");
    ASSERT_CMD_OK(r);
    ASSERT_OUT_CONTAINS(r, "closed");
}

static void test_status_shows_high_priority_section(void)
{
    TatrResult r = tatr("status");
    ASSERT_CMD_OK(r);
    ASSERT_OUT_CONTAINS(r, "High priority");
}

static void test_status_lists_critical_issue(void)
{
    TatrResult r = tatr("status");
    ASSERT_CMD_OK(r);
    ASSERT_OUT_CONTAINS(r, "in progress");
}

static void test_status_shows_recent_activity_section(void)
{
    TatrResult r = tatr("status");
    ASSERT_CMD_OK(r);
    ASSERT_OUT_CONTAINS(r, "Recent activity");
}

static void test_status_shows_top_tags_section(void)
{
    TatrResult r = tatr("status");
    ASSERT_CMD_OK(r);
    ASSERT_OUT_CONTAINS(r, "Top tags");
}

static void test_status_shows_backend_tag(void)
{
    TatrResult r = tatr("status");
    ASSERT_CMD_OK(r);
    ASSERT_OUT_CONTAINS(r, "backend");
}

static void test_status_stale_section_exists(void)
{
    TatrResult r = tatr("status");
    ASSERT_CMD_OK(r);
    ASSERT_OUT_CONTAINS(r, "Stale");
}

static void test_status_custom_limit(void)
{
    TatrResult r = tatr("status --limit 1");
    ASSERT_CMD_OK(r);
}

static void test_status_custom_stale_days(void)
{
    TatrResult r = tatr("status --stale-days 999");
    ASSERT_CMD_OK(r);
    ASSERT_OUT_CONTAINS(r, "none");
}

static void test_status_empty_repo(void)
{
    tatr_init();
    TatrResult r = tatr("status");
    ASSERT_CMD_OK(r);
    ASSERT_OUT_CONTAINS(r, "0");
}

static void test_status_no_repo_fails(void)
{
    TatrResult r = tatr("status");
    ASSERT_CMD_FAIL(r);
}

static void test_status_completion_percentage(void)
{
    TatrResult r = tatr("status");
    ASSERT_CMD_OK(r);
    ASSERT_OUT_CONTAINS(r, "%");
}

static void test_version_outputs_version_string(void)
{
    TatrResult r = tatr("version");
    ASSERT_CMD_OK(r);
    const char *combined = tatr_combined(&r);
    bool found = false;
    for (const char *p = combined; *p; p++) {
        if (*p >= '0' && *p <= '9' && *(p+1) == '.') {
            found = true;
            break;
        }
    }
    ASSERT_TRUE(found);
}

static void test_version_exit_zero(void)
{
    TatrResult r = tatr("version");
    ASSERT_EQ_INT(r.exit_code, 0);
}

static void test_help_global_lists_commands(void)
{
    TatrResult r = tatr("help");
    ASSERT_CMD_OK(r);
    ASSERT_OUT_CONTAINS(r, "new");
    ASSERT_OUT_CONTAINS(r, "list");
    ASSERT_OUT_CONTAINS(r, "search");
    ASSERT_OUT_CONTAINS(r, "attach");
}

static void test_help_per_command_new(void)
{
    TatrResult r = tatr("help new");
    ASSERT_CMD_OK(r);
    ASSERT_OUT_CONTAINS(r, "title");
}

static void test_help_per_command_list(void)
{
    TatrResult r = tatr("help list");
    ASSERT_CMD_OK(r);
    ASSERT_OUT_CONTAINS(r, "status");
}

static void test_help_unknown_command_fails(void)
{
    TatrResult r = tatr("help frobnicate");
    ASSERT_CMD_FAIL(r);
    ASSERT_OUT_CONTAINS(r, "unknown command");
}

static void test_help_exit_zero_on_global(void)
{
    TatrResult r = tatr("help");
    ASSERT_EQ_INT(r.exit_code, 0);
}

static void test_unknown_command_nonzero_exit(void)
{
    TatrResult r = tatr("frobnicate");
    ASSERT_NE_INT(r.exit_code, 0);
}

void suite_status_meta(void)
{
    SUITE("status");
    RUN_TEST(test_status_no_repo_fails);
    RUN_TEST(test_status_empty_repo);
    RUN_TEST_F(test_status_shows_total_count,          setup_mixed_repo, NULL);
    RUN_TEST_F(test_status_shows_open_count,           setup_mixed_repo, NULL);
    RUN_TEST_F(test_status_shows_closed_section,       setup_mixed_repo, NULL);
    RUN_TEST_F(test_status_shows_high_priority_section,setup_mixed_repo, NULL);
    RUN_TEST_F(test_status_lists_critical_issue,       setup_mixed_repo, NULL);
    RUN_TEST_F(test_status_shows_recent_activity_section, setup_mixed_repo, NULL);
    RUN_TEST_F(test_status_shows_top_tags_section,     setup_mixed_repo, NULL);
    RUN_TEST_F(test_status_shows_backend_tag,          setup_mixed_repo, NULL);
    RUN_TEST_F(test_status_stale_section_exists,       setup_mixed_repo, NULL);
    RUN_TEST_F(test_status_custom_limit,               setup_mixed_repo, NULL);
    RUN_TEST_F(test_status_custom_stale_days,          setup_mixed_repo, NULL);
    RUN_TEST_F(test_status_completion_percentage,      setup_mixed_repo, NULL);

    SUITE("version");
    RUN_TEST(test_version_outputs_version_string);
    RUN_TEST(test_version_exit_zero);

    SUITE("help");
    RUN_TEST(test_help_global_lists_commands);
    RUN_TEST(test_help_per_command_new);
    RUN_TEST(test_help_per_command_list);
    RUN_TEST(test_help_unknown_command_fails);
    RUN_TEST(test_help_exit_zero_on_global);

    SUITE("unknown-command");
    RUN_TEST(test_unknown_command_nonzero_exit);
}
