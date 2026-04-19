#include "test_helpers.h"

static char issue_a[64];
static char issue_b[64];

static void setup_two_issues(void)
{
    tatr_init();
    tatr_new_issue("--title 'issue alpha' --priority high --tag alpha --tag shared", issue_a, sizeof(issue_a));
    tatr_new_issue("--title 'issue beta' --priority low --tag beta --tag shared",    issue_b, sizeof(issue_b));
}

static void test_close_marks_closed(void)
{
    char args[128];
    snprintf(args, sizeof(args), "close %s", issue_a);
    TatrResult r = tatr(args);
    ASSERT_CMD_OK(r);
    ASSERT_OUT_CONTAINS(r, "Closed");

    char path[256];
    snprintf(path, sizeof(path), ".tatr/issues/%s/issue.tatr", issue_a);
    ASSERT_CONTAINS(tf_read_file(path), "status: closed");
}

static void test_close_wontfix_reason(void)
{
    char args[128];
    snprintf(args, sizeof(args), "close %s --reason wontfix", issue_a);
    TatrResult r = tatr(args);
    ASSERT_CMD_OK(r);

    char path[256];
    snprintf(path, sizeof(path), ".tatr/issues/%s/issue.tatr", issue_a);
    ASSERT_CONTAINS(tf_read_file(path), "status: wontfix");
}

static void test_close_unknown_id_fails(void)
{
    TatrResult r = tatr("close nonexistent_xyz");
    ASSERT_CMD_FAIL(r);
}

static void test_close_missing_id_fails(void)
{
    TatrResult r = tatr("close");
    ASSERT_CMD_FAIL(r);
}

static void test_close_hidden_from_list_after_close(void)
{
    char args[128];
    snprintf(args, sizeof(args), "close %s", issue_a);
    tatr(args);

    TatrResult r = tatr("list");
    ASSERT_OUT_NOT_CONTAINS(r, "issue alpha");
}

static void test_reopen_sets_open(void)
{
    char args[128];
    snprintf(args, sizeof(args), "close %s", issue_a);
    tatr(args);

    snprintf(args, sizeof(args), "reopen %s", issue_a);
    TatrResult r = tatr(args);
    ASSERT_CMD_OK(r);
    ASSERT_OUT_CONTAINS(r, "Reopened");

    char path[256];
    snprintf(path, sizeof(path), ".tatr/issues/%s/issue.tatr", issue_a);
    ASSERT_CONTAINS(tf_read_file(path), "status: open");
}

static void test_reopen_unknown_id_fails(void)
{
    TatrResult r = tatr("reopen ghost_id");
    ASSERT_CMD_FAIL(r);
}

static void test_reopen_missing_id_fails(void)
{
    TatrResult r = tatr("reopen");
    ASSERT_CMD_FAIL(r);
}

static void test_reopen_appears_in_list(void)
{
    char args[128];
    snprintf(args, sizeof(args), "close %s && true", issue_a); // ignore close result
    tatr(args);

    snprintf(args, sizeof(args), "reopen %s", issue_a);
    tatr(args);

    TatrResult r = tatr("list");
    ASSERT_OUT_CONTAINS(r, "issue alpha");
}

static void test_delete_removes_directory(void)
{
    char args[128];
    snprintf(args, sizeof(args), "delete %s", issue_a);
    TatrResult r = tatr(args);
    ASSERT_CMD_OK(r);

    char path[256];
    snprintf(path, sizeof(path), ".tatr/issues/%s", issue_a);
    ASSERT_PATH_NOT_EXISTS(path);
}

static void test_delete_unknown_id_fails(void)
{
    TatrResult r = tatr("delete ghost_issue");
    ASSERT_CMD_FAIL(r);
    ASSERT_OUT_CONTAINS(r, "not found");
}

static void test_delete_missing_id_fails(void)
{
    TatrResult r = tatr("delete");
    ASSERT_CMD_FAIL(r);
}

static void test_delete_not_in_list_after_delete(void)
{
    char args[128];
    snprintf(args, sizeof(args), "delete %s", issue_a);
    tatr(args);

    TatrResult r = tatr("list --all");
    ASSERT_OUT_NOT_CONTAINS(r, "issue alpha");
}

static void test_tag_add(void)
{
    char args[128];
    snprintf(args, sizeof(args), "tag %s --tag newtag", issue_a);
    TatrResult r = tatr(args);
    ASSERT_CMD_OK(r);

    char path[256];
    snprintf(path, sizeof(path), ".tatr/issues/%s/issue.tatr", issue_a);
    ASSERT_CONTAINS(tf_read_file(path), "newtag");
}

static void test_tag_add_preserves_existing(void)
{
    char args[128];
    snprintf(args, sizeof(args), "tag %s --tag newtag", issue_a);
    tatr(args);

    char path[256];
    snprintf(path, sizeof(path), ".tatr/issues/%s/issue.tatr", issue_a);
    const char *content = tf_read_file(path);
    ASSERT_CONTAINS(content, "alpha");   /* original */
    ASSERT_CONTAINS(content, "newtag"); /* added */
}

static void test_tag_remove(void)
{
    char args[256];
    snprintf(args, sizeof(args), "tag %s --tag alpha --remove", issue_a);
    TatrResult r = tatr(args);
    ASSERT_CMD_OK(r);

    char path[256];
    snprintf(path, sizeof(path), ".tatr/issues/%s/issue.tatr", issue_a);
    const char *content = tf_read_file(path);
    const char *tags_line = strstr(content, "tags:");
    if (tags_line) {
        char line[256] = {0};
        sscanf(tags_line, "tags: %255[^\n]", line);
        ASSERT_NOT_CONTAINS(line, "alpha");
    }
}

static void test_tag_remove_nonexistent_warns(void)
{
    char args[128];
    snprintf(args, sizeof(args), "tag %s --tag nothere --remove", issue_a);
    TatrResult r = tatr(args);
    ASSERT_OUT_CONTAINS(r, "not present");
}

static void test_tag_duplicate_add_warns(void)
{
    char args[128];
    snprintf(args, sizeof(args), "tag %s --tag alpha", issue_a);
    TatrResult r = tatr(args);
    ASSERT_OUT_CONTAINS(r, "already present");
}

static void test_tag_multiple_at_once(void)
{
    char args[256];
    snprintf(args, sizeof(args), "tag %s --tag x,y,z", issue_a);
    TatrResult r = tatr(args);
    ASSERT_CMD_OK(r);

    char path[256];
    snprintf(path, sizeof(path), ".tatr/issues/%s/issue.tatr", issue_a);
    const char *content = tf_read_file(path);
    ASSERT_CONTAINS(content, "x");
    ASSERT_CONTAINS(content, "y");
    ASSERT_CONTAINS(content, "z");
}

static void test_comment_appends_to_file(void)
{
    char args[256];
    snprintf(args, sizeof(args),
             "comment %s --message 'Found root cause'", issue_a);
    TatrResult r = tatr(args);
    ASSERT_CMD_OK(r);
    ASSERT_OUT_CONTAINS(r, "Comment added");

    char path[256];
    snprintf(path, sizeof(path), ".tatr/issues/%s/issue.tatr", issue_a);
    ASSERT_CONTAINS(tf_read_file(path), "Found root cause");
}

static void test_comment_includes_date(void)
{
    char args[256];
    snprintf(args, sizeof(args), "comment %s --message 'ts check'", issue_a);
    tatr(args);

    char path[256];
    snprintf(path, sizeof(path), ".tatr/issues/%s/issue.tatr", issue_a);
    ASSERT_CONTAINS(tf_read_file(path), "date:");
}

static void test_comment_with_author(void)
{
    char args[256];
    snprintf(args, sizeof(args), "comment %s --message 'authored note' --author alice", issue_a);
    tatr(args);

    char path[256];
    snprintf(path, sizeof(path), ".tatr/issues/%s/issue.tatr", issue_a);
    const char *content = tf_read_file(path);
    ASSERT_CONTAINS(content, "author: alice");
    ASSERT_CONTAINS(content, "authored note");
}

static void test_comment_missing_message_fails(void)
{
    char args[128];
    snprintf(args, sizeof(args), "comment %s", issue_a);
    TatrResult r = tatr(args);
    ASSERT_CMD_FAIL(r);
}

static void test_comment_multiple_appended(void)
{
    char args[256];
    snprintf(args, sizeof(args), "comment %s --message 'first'", issue_a);
    tatr(args);
    snprintf(args, sizeof(args), "comment %s --message 'second'", issue_a);
    tatr(args);

    char path[256];
    snprintf(path, sizeof(path), ".tatr/issues/%s/issue.tatr", issue_a);
    const char *content = tf_read_file(path);
    ASSERT_CONTAINS(content, "first");
    ASSERT_CONTAINS(content, "second");
}

static void test_search_finds_title_match(void)
{
    TatrResult r = tatr("search alpha");
    ASSERT_CMD_OK(r);
    ASSERT_OUT_CONTAINS(r, "issue alpha");
}

static void test_search_no_match_message(void)
{
    TatrResult r = tatr("search xyznomatch");
    ASSERT_OUT_CONTAINS(r, "no results");
}

static void test_search_case_insensitive_default(void)
{
    TatrResult r = tatr("search ALPHA");
    ASSERT_CMD_OK(r);
    ASSERT_OUT_CONTAINS(r, "issue alpha");
}

static void test_search_case_sensitive_no_match(void)
{
    TatrResult r = tatr("search --case ALPHA");
    ASSERT_OUT_NOT_CONTAINS(r, "issue alpha");
}

static void test_search_field_filter_tag(void)
{
    TatrResult r = tatr("search tag:alpha");
    ASSERT_CMD_OK(r);
    ASSERT_OUT_CONTAINS(r, "issue alpha");
    ASSERT_OUT_NOT_CONTAINS(r, "issue beta");
}

static void test_search_field_filter_status(void)
{
    TatrResult r = tatr("search status:open");
    ASSERT_CMD_OK(r);
    ASSERT_OUT_CONTAINS(r, "issue alpha");
    ASSERT_OUT_CONTAINS(r, "issue beta");
}

static void test_search_field_filter_priority(void)
{
    TatrResult r = tatr("search priority:high");
    ASSERT_CMD_OK(r);
    ASSERT_OUT_CONTAINS(r, "issue alpha");
    ASSERT_OUT_NOT_CONTAINS(r, "issue beta");
}

static void test_search_multi_token_and(void)
{
    TatrResult r = tatr("search alpha shared");
    ASSERT_CMD_OK(r);
    ASSERT_OUT_CONTAINS(r, "issue alpha");
    ASSERT_OUT_NOT_CONTAINS(r, "issue beta");
}

static void test_search_limit(void)
{
    TatrResult r = tatr("search shared --limit 1");
    ASSERT_CMD_OK(r);
    ASSERT_OUT_CONTAINS(r, "1 result");
}

static void test_search_header_only(void)
{
    char args[256];
    snprintf(args, sizeof(args), "comment %s --message 'unique_body_phrase'", issue_b);
    tatr(args);

    TatrResult r = tatr("search --header unique_body_phrase");
    ASSERT_OUT_NOT_CONTAINS(r, "issue beta");
}

static void test_search_missing_query_fails(void)
{
    TatrResult r = tatr("search");
    ASSERT_CMD_FAIL(r);
}

void suite_lifecycle_org(void)
{
    SUITE("close");
    RUN_TEST_F(test_close_marks_closed,          setup_two_issues, NULL);
    RUN_TEST_F(test_close_wontfix_reason,        setup_two_issues, NULL);
    RUN_TEST_F(test_close_unknown_id_fails,      setup_two_issues, NULL);
    RUN_TEST_F(test_close_missing_id_fails,      setup_two_issues, NULL);
    RUN_TEST_F(test_close_hidden_from_list_after_close, setup_two_issues, NULL);

    SUITE("reopen");
    RUN_TEST_F(test_reopen_sets_open,            setup_two_issues, NULL);
    RUN_TEST_F(test_reopen_unknown_id_fails,     setup_two_issues, NULL);
    RUN_TEST_F(test_reopen_missing_id_fails,     setup_two_issues, NULL);
    RUN_TEST_F(test_reopen_appears_in_list,      setup_two_issues, NULL);

    SUITE("delete");
    RUN_TEST_F(test_delete_removes_directory,    setup_two_issues, NULL);
    RUN_TEST_F(test_delete_unknown_id_fails,     setup_two_issues, NULL);
    RUN_TEST_F(test_delete_missing_id_fails,     setup_two_issues, NULL);
    RUN_TEST_F(test_delete_not_in_list_after_delete, setup_two_issues, NULL);

    SUITE("tag");
    RUN_TEST_F(test_tag_add,                     setup_two_issues, NULL);
    RUN_TEST_F(test_tag_add_preserves_existing,  setup_two_issues, NULL);
    RUN_TEST_F(test_tag_remove,                  setup_two_issues, NULL);
    RUN_TEST_F(test_tag_remove_nonexistent_warns,setup_two_issues, NULL);
    RUN_TEST_F(test_tag_duplicate_add_warns,     setup_two_issues, NULL);
    RUN_TEST_F(test_tag_multiple_at_once,        setup_two_issues, NULL);

    SUITE("comment");
    RUN_TEST_F(test_comment_appends_to_file,     setup_two_issues, NULL);
    RUN_TEST_F(test_comment_includes_date,       setup_two_issues, NULL);
    RUN_TEST_F(test_comment_with_author,         setup_two_issues, NULL);
    RUN_TEST_F(test_comment_missing_message_fails, setup_two_issues, NULL);
    RUN_TEST_F(test_comment_multiple_appended,   setup_two_issues, NULL);

    SUITE("search");
    RUN_TEST_F(test_search_finds_title_match,    setup_two_issues, NULL);
    RUN_TEST_F(test_search_no_match_message,     setup_two_issues, NULL);
    RUN_TEST_F(test_search_case_insensitive_default, setup_two_issues, NULL);
    RUN_TEST_F(test_search_case_sensitive_no_match,  setup_two_issues, NULL);
    RUN_TEST_F(test_search_field_filter_tag,     setup_two_issues, NULL);
    RUN_TEST_F(test_search_field_filter_status,  setup_two_issues, NULL);
    RUN_TEST_F(test_search_field_filter_priority,setup_two_issues, NULL);
    RUN_TEST_F(test_search_multi_token_and,      setup_two_issues, NULL);
    RUN_TEST_F(test_search_limit,                setup_two_issues, NULL);
    RUN_TEST_F(test_search_header_only,          setup_two_issues, NULL);
    RUN_TEST_F(test_search_missing_query_fails,  setup_two_issues, NULL);
}
