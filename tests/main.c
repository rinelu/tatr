#include <stdint.h>
#define CLAG_IMPLEMENTATION
#include "../libs/clag.h"

#define LOG_GLOBAL
#define LOG_IMPLEMENTATION

#include "test_framework.h"

TF_State       _tf     = {0};
TF_SB_Registry _tf_sbs = {0};
TF_TestCases _tf_tests = {0};
bool delete_fail_tmp   = true;
volatile sig_atomic_t 
     tf_interrupted    = false;

void suite_init_new(void);
void suite_list_show_edit(void);
void suite_lifecycle_org(void);
void suite_attachments(void);
void suite_status_meta(void);

int main(int argc, char **argv)
{
    bool *dont_delete  = clag_bool("delete-fail-tmp", 'D', false,        "Don't delete the tmp directory when failing the test");
    char **out_to_file = clag_str("out-to-file",      'f', "tt_log.txt", "Output the log message to a file");
    int64_t *jobs      = clag_int64("jobs",           'j', 4,            "Allow N jobs at once;");
    clag_parse(argc, argv);
    delete_fail_tmp = !*dont_delete;

    FILE *f = NULL;
    if (clag_was_seen("out-to-file")) {
        f = fopen(*out_to_file, "w");
        if (f == NULL)
            printf("Could not open file %s for writing: %s\n", *out_to_file, strerror(errno));
    }
    log_init(.file=f);

    suite_init_new();
    suite_list_show_edit();
    suite_lifecycle_org();
    suite_attachments();
    suite_status_meta();

    tf_install_signal_handler();
    tf_run_all_parallel((int)*jobs);

    int r = tf_report();
    da_free(_tf_tests);
    if (f) fclose(f);
    return r;
}
