#include "test_helpers.h"

static void test_init_creates_directory(void)
{
    Tatr_Result r = tatr("init");
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
}

static void test_init_fails_if_already_exists(void)
{
    tatr("init");
    Tatr_Result r = tatr("init");
    ASSERT_CMD_FAIL(r);
    ASSERT_OUT_CONTAINS(r, "already exists");
}

static void test_init_force_reinitializes(void)
{
    tatr("init");
    Tatr_Result r = tatr("init --force");
    ASSERT_CMD_OK(r);
}

static void test_init_short_flag_force(void)
{
    tatr("init");
    Tatr_Result r = tatr("init -f");
    ASSERT_CMD_OK(r);
}

static void test_init_writes_schema_version(void)
{
    tatr("init");
    const char *v = tf_read_file(".tatr/VERSION");
    ASSERT_NOT_NULL(v);
    ASSERT_CONTAINS(v, "1");
}

static void test_legacy_repo_is_migrated_on_first_use(void)
{
    tatr("init");
    remove(".tatr/VERSION"); /* simulate a pre-schema-versioning repo */

    Tatr_Result r = tatr("new -t \"legacy repo issue\"");
    ASSERT_CMD_OK(r);
    ASSERT_OUT_CONTAINS(r, "migrated repo schema 0 -> 1");

    const char *v = tf_read_file(".tatr/VERSION");
    ASSERT_NOT_NULL(v);
    ASSERT_CONTAINS(v, "1");

    /* second command against the now-current repo: no migration noise */
    Tatr_Result r2 = tatr("list --no-header");
    ASSERT_CMD_OK(r2);
    ASSERT_OUT_NOT_CONTAINS(r2, "migrated");
}

static void test_future_schema_version_is_rejected(void)
{
    tatr("init");
    tf_write_file(".tatr/VERSION", "999");

    Tatr_Result r = tatr("list");
    ASSERT_CMD_FAIL(r);
    ASSERT_OUT_CONTAINS(r, "newer than this build of tatr understands");
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
    Tatr_Result r = tatr("new --title 'bad' --priority ultra");
    ASSERT_CMD_FAIL(r);
    ASSERT_OUT_CONTAINS(r, "invalid value");
}

static void test_new_invalid_status_rejected(void)
{
    Tatr_Result r = tatr("new --title 'bad' --status limbo");
    ASSERT_CMD_FAIL(r);
    ASSERT_OUT_CONTAINS(r, "invalid value");
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
    Tatr_Result r = tatr("new --title 'no repo'");
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
    RUN_TEST(test_init_writes_schema_version);
    RUN_TEST(test_legacy_repo_is_migrated_on_first_use);
    RUN_TEST(test_future_schema_version_is_rejected);

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
