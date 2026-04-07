#include "test_helpers.h"

static char issue_id[64];

static void setup_issue_with_files(void)
{
    tatr_init();
    tatr_new_issue("--title 'attach target'", issue_id, sizeof(issue_id));

    tf_write_file("report.pdf",   "FAKE_PDF_CONTENT");
    tf_write_file("screenshot.png", "FAKE_PNG_CONTENT");
    tf_write_file("notes.txt",    "Some notes.");
}

static void test_attach_single_file(void)
{
    char args[256];
    snprintf(args, sizeof(args), "attach %s report.pdf", issue_id);
    TatrResult r = tatr(args);
    ASSERT_CMD_OK(r);
    ASSERT_OUT_CONTAINS(r, "Attached");

    char path[256];
    snprintf(path, sizeof(path), ".tatr/issues/%s/attachments/report.pdf", issue_id);
    ASSERT_PATH_EXISTS(path);
}

static void test_attach_file_content_preserved(void)
{
    char args[256];
    snprintf(args, sizeof(args), "attach %s notes.txt", issue_id);
    tatr(args);

    char path[256];
    snprintf(path, sizeof(path), ".tatr/issues/%s/attachments/notes.txt", issue_id);
    ASSERT_CONTAINS(tf_read_file(path), "Some notes.");
}

static void test_attach_multiple_files(void)
{
    char args[256];
    snprintf(args, sizeof(args),
             "attach %s report.pdf screenshot.png notes.txt", issue_id);
    TatrResult r = tatr(args);
    ASSERT_CMD_OK(r);

    char path[256];
    snprintf(path, sizeof(path), ".tatr/issues/%s/attachments/report.pdf", issue_id);
    ASSERT_PATH_EXISTS(path);
    snprintf(path, sizeof(path), ".tatr/issues/%s/attachments/screenshot.png", issue_id);
    ASSERT_PATH_EXISTS(path);
    snprintf(path, sizeof(path), ".tatr/issues/%s/attachments/notes.txt", issue_id);
    ASSERT_PATH_EXISTS(path);
}

static void test_attach_nonexistent_file_fails(void)
{
    char args[256];
    snprintf(args, sizeof(args), "attach %s ghost.bin", issue_id);
    TatrResult r = tatr(args);
    ASSERT_CMD_FAIL(r);
    ASSERT_OUT_CONTAINS(r, "not found");
}

static void test_attach_unknown_issue_fails(void)
{
    TatrResult r = tatr("attach bad_id notes.txt");
    ASSERT_CMD_FAIL(r);
    ASSERT_OUT_CONTAINS(r, "not found");
}

static void test_attach_missing_args_fails(void)
{
    char args[128];
    snprintf(args, sizeof(args), "attach %s", issue_id);
    TatrResult r = tatr(args);
    ASSERT_CMD_FAIL(r);
}

static void test_attach_no_args_fails(void)
{
    TatrResult r = tatr("attach");
    ASSERT_CMD_FAIL(r);
}

static void test_attach_conflict_renamed(void)
{
    char args[256];
    snprintf(args, sizeof(args), "attach %s notes.txt", issue_id);
    tatr(args);
    TatrResult r = tatr(args);
    ASSERT_CMD_OK(r);
    ASSERT_OUT_CONTAINS(r, "renamed");
}

static void test_attachls_empty(void)
{
    char args[128];
    snprintf(args, sizeof(args), "attachls %s", issue_id);
    TatrResult r = tatr(args);
    ASSERT_CMD_OK(r);
    ASSERT_OUT_CONTAINS(r, "no attachments");
}

static void test_attachls_lists_files(void)
{
    char args[256];
    snprintf(args, sizeof(args), "attach %s report.pdf notes.txt", issue_id);
    tatr(args);

    snprintf(args, sizeof(args), "attachls %s", issue_id);
    TatrResult r = tatr(args);
    ASSERT_CMD_OK(r);
    ASSERT_OUT_CONTAINS(r, "report.pdf");
    ASSERT_OUT_CONTAINS(r, "notes.txt");
}

static void test_attachls_unknown_issue_fails(void)
{
    TatrResult r = tatr("attachls ghost_id");
    ASSERT_CMD_FAIL(r);
    ASSERT_OUT_CONTAINS(r, "not found");
}

static void test_attachls_missing_id_fails(void)
{
    TatrResult r = tatr("attachls");
    ASSERT_CMD_FAIL(r);
}

static void test_detach_removes_file(void)
{
    char args[256];
    snprintf(args, sizeof(args), "attach %s notes.txt", issue_id);
    tatr(args);

    snprintf(args, sizeof(args), "detach %s notes.txt --yes", issue_id);
    TatrResult r = tatr(args);
    ASSERT_CMD_OK(r);
    ASSERT_OUT_CONTAINS(r, "Removed");

    char path[256];
    snprintf(path, sizeof(path), ".tatr/issues/%s/attachments/notes.txt", issue_id);
    ASSERT_PATH_NOT_EXISTS(path);
}

static void test_detach_nonexistent_file_fails(void)
{
    char args[256];
    snprintf(args, sizeof(args), "detach %s ghost.txt --yes", issue_id);
    TatrResult r = tatr(args);
    ASSERT_CMD_FAIL(r);
    ASSERT_OUT_CONTAINS(r, "not found");
}

static void test_detach_unknown_issue_fails(void)
{
    TatrResult r = tatr("detach ghost_id notes.txt --yes");
    ASSERT_CMD_FAIL(r);
    ASSERT_OUT_CONTAINS(r, "not found");
}

static void test_detach_missing_args_fails(void)
{
    char args[128];
    snprintf(args, sizeof(args), "detach %s", issue_id);
    TatrResult r = tatr(args);
    ASSERT_CMD_FAIL(r);
}

static void test_detach_no_args_fails(void)
{
    TatrResult r = tatr("detach");
    ASSERT_CMD_FAIL(r);
}

static void test_detach_does_not_affect_other_files(void)
{
    char args[256];
    snprintf(args, sizeof(args),
             "attach %s report.pdf notes.txt", issue_id);
    tatr(args);

    snprintf(args, sizeof(args), "detach %s notes.txt --yes", issue_id);
    tatr(args);

    char path[256];
    snprintf(path, sizeof(path),
             ".tatr/issues/%s/attachments/report.pdf", issue_id);
    ASSERT_PATH_EXISTS(path);
}

static void test_detach_not_listed_in_attachls_after(void)
{
    char args[256];
    snprintf(args, sizeof(args), "attach %s notes.txt", issue_id);
    tatr(args);
    snprintf(args, sizeof(args), "detach %s notes.txt --yes", issue_id);
    tatr(args);

    snprintf(args, sizeof(args), "attachls %s", issue_id);
    TatrResult r = tatr(args);
    ASSERT_OUT_NOT_CONTAINS(r, "notes.txt");
}

void suite_attachments(void)
{
    SUITE("attach");
    RUN_TEST_F(test_attach_single_file,          setup_issue_with_files, NULL);
    RUN_TEST_F(test_attach_file_content_preserved, setup_issue_with_files, NULL);
    RUN_TEST_F(test_attach_multiple_files,       setup_issue_with_files, NULL);
    RUN_TEST_F(test_attach_nonexistent_file_fails, setup_issue_with_files, NULL);
    RUN_TEST_F(test_attach_unknown_issue_fails,  setup_issue_with_files, NULL);
    RUN_TEST_F(test_attach_missing_args_fails,   setup_issue_with_files, NULL);
    RUN_TEST_F(test_attach_no_args_fails,        setup_issue_with_files, NULL);
    RUN_TEST_F(test_attach_conflict_renamed,     setup_issue_with_files, NULL);

    SUITE("attachls");
    RUN_TEST_F(test_attachls_empty,              setup_issue_with_files, NULL);
    RUN_TEST_F(test_attachls_lists_files,        setup_issue_with_files, NULL);
    RUN_TEST_F(test_attachls_unknown_issue_fails,setup_issue_with_files, NULL);
    RUN_TEST_F(test_attachls_missing_id_fails,   setup_issue_with_files, NULL);

    SUITE("detach");
    RUN_TEST_F(test_detach_removes_file,                    setup_issue_with_files, NULL);
    RUN_TEST_F(test_detach_nonexistent_file_fails,          setup_issue_with_files, NULL);
    RUN_TEST_F(test_detach_unknown_issue_fails,             setup_issue_with_files, NULL);
    RUN_TEST_F(test_detach_missing_args_fails,              setup_issue_with_files, NULL);
    RUN_TEST_F(test_detach_no_args_fails,                   setup_issue_with_files, NULL);
    RUN_TEST_F(test_detach_does_not_affect_other_files,     setup_issue_with_files, NULL);
    RUN_TEST_F(test_detach_not_listed_in_attachls_after,    setup_issue_with_files, NULL);
}
