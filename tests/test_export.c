#include "test_helpers.h"

static char issue_id[64];

static void setup_issue(void)
{
    tatr_init();

    ASSERT_TRUE(tatr_new_issue(
        "--title 'export test' "
        "--body 'Hello world\n---comment---\ndate: 2024\nauthor: me\nhi there\n' "
        "--tag bug --tag backend",
        issue_id,
        sizeof(issue_id)
    ));
}

static void test_export_markdown_stdout(void)
{
    char cmd[128];
    snprintf(cmd, sizeof(cmd), "export %s", issue_id);

    TatrResult r = tatr(cmd);
    ASSERT_CMD_OK(r);

    ASSERT_OUT_CONTAINS(r, "# export test");
    ASSERT_OUT_CONTAINS(r, "## Description");
    ASSERT_OUT_CONTAINS(r, "Hello world");
}

static void test_export_json_stdout(void)
{
    char cmd[128];
    snprintf(cmd, sizeof(cmd), "export %s --format json", issue_id);

    TatrResult r = tatr(cmd);
    ASSERT_CMD_OK(r);

    ASSERT_OUT_CONTAINS(r, "\"title\": \"export test\"");
    ASSERT_OUT_CONTAINS(r, "\"tags\":");
    ASSERT_OUT_CONTAINS(r, "\"comments\":");
}

static void test_export_json_pretty(void)
{
    char cmd[128];
    snprintf(cmd, sizeof(cmd), "export %s --format json --pretty", issue_id);

    TatrResult r = tatr(cmd);
    ASSERT_CMD_OK(r);

    // pretty JSON should contain newlines + indentation
    ASSERT_OUT_CONTAINS(r, "\n");
    ASSERT_OUT_CONTAINS(r, "  \"title\"");
}

static void test_export_json_minified(void)
{
    char cmd[128];
    snprintf(cmd, sizeof(cmd), "export %s --format json --minify", issue_id);

    TatrResult r = tatr(cmd);
    ASSERT_CMD_OK(r);

    // minified JSON should NOT contain indentation spaces
    ASSERT_OUT_NOT_CONTAINS(r, "  \"title\"");
}

static void test_export_to_file(void)
{
    char cmd[256];
    snprintf(cmd, sizeof(cmd), "export %s --format json --output out.json", issue_id);

    TatrResult r = tatr(cmd);
    ASSERT_CMD_OK(r);

    ASSERT_PATH_EXISTS("out.json");

    const char *content = tf_read_file("out.json");
    ASSERT_NOT_NULL(content);
    ASSERT_CONTAINS(content, "\"title\": \"export test\"");
}

static void test_export_invalid_format(void)
{
    char cmd[128];
    snprintf(cmd, sizeof(cmd), "export %s --format xml", issue_id);

    TatrResult r = tatr(cmd);
    ASSERT_CMD_FAIL(r);

    ASSERT_OUT_CONTAINS(r, "invalid value");
    ASSERT_OUT_CONTAINS(r, "valid");
}

static void test_export_list_formats(void)
{
    TatrResult r = tatr("export --list-format");
    ASSERT_CMD_OK(r);

    ASSERT_OUT_CONTAINS(r, "markdown");
    ASSERT_OUT_CONTAINS(r, "json");
}

static void test_export_unknown_issue_fails(void)
{
    TatrResult r = tatr("export bad_id");
    ASSERT_CMD_FAIL(r);
    ASSERT_OUT_CONTAINS(r, "not found");
}

static void test_export_missing_id_fails(void)
{
    TatrResult r = tatr("export");
    ASSERT_CMD_FAIL(r);
}

static void test_export_tags_rendered_json(void)
{
    char cmd[128];
    snprintf(cmd, sizeof(cmd), "export %s --format json", issue_id);

    TatrResult r = tatr(cmd);
    ASSERT_CMD_OK(r);

    ASSERT_OUT_CONTAINS(r, "\"bug\"");
    ASSERT_OUT_CONTAINS(r, "\"backend\"");
}

static void test_export_comments_parsed(void)
{
    char cmd[128];
    snprintf(cmd, sizeof(cmd), "export %s --format json", issue_id);

    TatrResult r = tatr(cmd);
    ASSERT_CMD_OK(r);

    ASSERT_OUT_CONTAINS(r, "\"date\": \"2024\"");
    ASSERT_OUT_CONTAINS(r, "\"author\": \"me\"");
    ASSERT_OUT_CONTAINS(r, "\"body\": \"hi there");
}

void suite_export(void)
{
    SUITE("export");

    RUN_TEST_F(test_export_markdown_stdout, setup_issue, NULL);
    RUN_TEST_F(test_export_json_stdout,     setup_issue, NULL);
    RUN_TEST_F(test_export_json_pretty,     setup_issue, NULL);
    RUN_TEST_F(test_export_json_minified,   setup_issue, NULL);
    RUN_TEST_F(test_export_to_file,         setup_issue, NULL);
    RUN_TEST_F(test_export_invalid_format,  setup_issue, NULL);
    RUN_TEST(test_export_list_formats);
    RUN_TEST_F(test_export_unknown_issue_fails, setup_issue, NULL);
    RUN_TEST(test_export_missing_id_fails);
    RUN_TEST_F(test_export_tags_rendered_json, setup_issue, NULL);
    RUN_TEST_F(test_export_comments_parsed,    setup_issue, NULL);
}
