#define CLAG_IMPLEMENTATION
#include "../libs/clag.h"

#define LOG_GLOBAL
#define LOG_IMPL_GLOBAL

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
    bool *dont_delete = clag_bool("delete_fail_tmp", 'D', false, "Don't delete the tmp directory when failing the test");
    clag_parse(argc, argv);
    delete_fail_tmp = !*dont_delete;

    log_init(.use_color=true);

    log_msg(TF_BOLD "\ntatr test suite" TF_RESET);

    suite_init_new();
    suite_list_show_edit();
    suite_lifecycle_org();
    suite_attachments();
    suite_status_meta();

    tf_install_signal_handler();
    tf_run_all_parallel(4);

    int r = tf_report();
    da_free(_tf_tests);
    return r;
}
