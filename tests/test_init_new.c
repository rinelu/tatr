#include "test_helpers.h"

static void test_init_creates_directory(void)
{
    TatrResult r = tatr("init");
    ASSERT_CMD_OK(r);
    ASSERT_PATH_EXISTS(".tatr");
    ASSERT_PATH_EXISTS(".tatr/issues");
    ASSERT_PATH_EXISTS(".tatr/config");
}

static void test_init_writes_default_config(void)
{
    tatr("init");
    const char *cfg = tf_read_file(".tatr/config");
    ASSERT_NOT_NULL(cfg);
    ASSERT_CONTAINS(cfg, "default_status");
    ASSERT_CONTAINS(cfg, "default_priority");
}

static void test_init_fails_if_already_exists(void)
{
    tatr("init");
    TatrResult r = tatr("init");
    ASSERT_CMD_FAIL(r);
    ASSERT_OUT_CONTAINS(r, "already exists");
}

static void test_init_force_reinitializes(void)
{
    tatr("init");
    TatrResult r = tatr("init --force");
    ASSERT_CMD_OK(r);
}

static void test_init_short_flag_force(void)
{
    tatr("init");
    TatrResult r = tatr("init -f");
    ASSERT_CMD_OK(r);
}

static void setup_repo(void) { tatr_init(); }

static void test_new_creates_issue(void)
{
    char id[64];
    ASSERT_TRUE(tatr_new_issue("--title 'login bug'", id, sizeof(id)));
    ASSERT_TRUE(strlen(id) > 0);

    char path[256];
    snprintf(path, sizeof(path), ".tatr/issues/%s/issue.tatr", id);
    ASSERT_PATH_EXISTS(path);
}

static void test_new_creates_attachments_dir(void)
{
    char id[64];
    tatr_new_issue("--title 'attach test'", id, sizeof(id));

    char path[256];
    snprintf(path, sizeof(path), ".tatr/issues/%s/attachments", id);
    ASSERT_PATH_EXISTS(path);
}

static void test_new_default_priority_normal(void)
{
    char id[64];
    tatr_new_issue("--title 'defaults test'", id, sizeof(id));

    char path[256];
    snprintf(path, sizeof(path), ".tatr/issues/%s/issue.tatr", id);
    const char *content = tf_read_file(path);
    ASSERT_CONTAINS(content, "priority: normal");
}

static void test_new_default_status_open(void)
{
    char id[64];
    tatr_new_issue("--title 'status test'", id, sizeof(id));

    char path[256];
    snprintf(path, sizeof(path), ".tatr/issues/%s/issue.tatr", id);
    const char *content = tf_read_file(path);
    ASSERT_CONTAINS(content, "status: open");
}

static void test_new_custom_priority(void)
{
    char id[64];
    tatr_new_issue("--title 'urgent' --priority critical", id, sizeof(id));

    char path[256];
    snprintf(path, sizeof(path), ".tatr/issues/%s/issue.tatr", id);
    ASSERT_CONTAINS(tf_read_file(path), "priority: critical");
}

static void test_new_custom_status(void)
{
    char id[64];
    tatr_new_issue("--title 'wip' --status in-progress", id, sizeof(id));

    char path[256];
    snprintf(path, sizeof(path), ".tatr/issues/%s/issue.tatr", id);
    ASSERT_CONTAINS(tf_read_file(path), "status: in-progress");
}

static void test_new_invalid_priority_rejected(void)
{
    TatrResult r = tatr("new --title 'bad' --priority ultra");
    ASSERT_CMD_FAIL(r);
    ASSERT_OUT_CONTAINS(r, "Invalid priority");
}

static void test_new_invalid_status_rejected(void)
{
    TatrResult r = tatr("new --title 'bad' --status limbo");
    ASSERT_CMD_FAIL(r);
    ASSERT_OUT_CONTAINS(r, "Invalid status");
}

static void test_new_with_body(void)
{
    char id[64];
    tatr_new_issue("--title 'body test' --body 'Steps to reproduce: ...'", id, sizeof(id));

    char path[256];
    snprintf(path, sizeof(path), ".tatr/issues/%s/issue.tatr", id);
    ASSERT_CONTAINS(tf_read_file(path), "Steps to reproduce");
}

static void test_new_with_single_tag(void)
{
    char id[64];
    tatr_new_issue("--title 'tagged' --tag backend", id, sizeof(id));

    char path[256];
    snprintf(path, sizeof(path), ".tatr/issues/%s/issue.tatr", id);
    ASSERT_CONTAINS(tf_read_file(path), "backend");
}

static void test_new_with_multiple_tags(void)
{
    char id[64];
    tatr_new_issue("--title 'multi-tag' --tag frontend --tag css --tag bug", id, sizeof(id));

    char path[256];
    snprintf(path, sizeof(path), ".tatr/issues/%s/issue.tatr", id);
    const char *content = tf_read_file(path);
    ASSERT_CONTAINS(content, "frontend");
    ASSERT_CONTAINS(content, "css");
    ASSERT_CONTAINS(content, "bug");
}

static void test_new_has_created_timestamp(void)
{
    char id[64];
    tatr_new_issue("--title 'ts test'", id, sizeof(id));

    char path[256];
    snprintf(path, sizeof(path), ".tatr/issues/%s/issue.tatr", id);
    ASSERT_CONTAINS(tf_read_file(path), "created:");
}

static void test_new_without_repo_fails(void)
{
    TatrResult r = tatr("new --title 'no repo'");
    ASSERT_CMD_FAIL(r);
}

static void test_new_body_from_file(void)
{
    tatr_init();
    tf_write_file("body.txt", "Long description from file.\n");

    char id[64];
    tatr_new_issue("--title 'file body' --file body.txt", id, sizeof(id));

    char path[256];
    snprintf(path, sizeof(path), ".tatr/issues/%s/issue.tatr", id);
    ASSERT_CONTAINS(tf_read_file(path), "Long description from file.");
}

void suite_init_new(void)
{
    SUITE("init");
    RUN_TEST(test_init_creates_directory);
    RUN_TEST(test_init_writes_default_config);
    RUN_TEST(test_init_fails_if_already_exists);
    RUN_TEST(test_init_force_reinitializes);
    RUN_TEST(test_init_short_flag_force);

    SUITE("new");
    RUN_TEST(test_new_without_repo_fails);
    RUN_TEST(test_new_body_from_file);
    RUN_TEST_F(test_new_creates_issue,             setup_repo, NULL);
    RUN_TEST_F(test_new_creates_attachments_dir,   setup_repo, NULL);
    RUN_TEST_F(test_new_default_priority_normal,   setup_repo, NULL);
    RUN_TEST_F(test_new_default_status_open,       setup_repo, NULL);
    RUN_TEST_F(test_new_custom_priority,           setup_repo, NULL);
    RUN_TEST_F(test_new_custom_status,             setup_repo, NULL);
    RUN_TEST_F(test_new_invalid_priority_rejected, setup_repo, NULL);
    RUN_TEST_F(test_new_invalid_status_rejected,   setup_repo, NULL);
    RUN_TEST_F(test_new_with_body,                 setup_repo, NULL);
    RUN_TEST_F(test_new_with_single_tag,           setup_repo, NULL);
    RUN_TEST_F(test_new_with_multiple_tags,        setup_repo, NULL);
    RUN_TEST_F(test_new_has_created_timestamp,     setup_repo, NULL);
}
