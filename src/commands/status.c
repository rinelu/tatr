#include "cmd.h"
#include "tatr.h"

int cmd_status(int argc, char **argv)
{
    uint64_t *limit      = clag_uint64("limit", 'n', 5, "Recent issues limit");
    uint64_t *stale_days = clag_uint64("stale-days", 0, 7, "Days before issue is stale");

    clag_usage("[options]");

    if (!clag_parse(argc, argv)) {
        clag_print_error(stderr);
        return 1;
    }
    if (!require_repo()) return 1;

    Temp_Checkpoint tmark = temp_save();
    const Renderer *r = renderer_for(TATR_FMT_HUMAN);

    Tatr_Status_Query q = {
        .recent_limit = *limit,
        .stale_days   = *stale_days,
    };

    Tatr_Status_Result result;
    Tatr_Error err = tatr_status(&q, &result);

    if (err != TATR_OK) {
        log_error("cannot read issues directory");
        temp_rewind(tmark);
        return 1;
    }

    r->status(stdout, &result);

    temp_rewind(tmark);
    return 0;
}
