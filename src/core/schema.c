#include "schema.h"

#include "global.h"
#include "fs.h"
#include "log.h"
#include "astring.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ULL unsigned long long

static bool schema_read_version(uint64_t *out)
{
    String_Builder sb = {0};

    if (!fs_read_file(TATR_VERSION_PATH, &sb)) {
        *out = 0;
        return true;
    }

    sb_append_null(&sb);
    char *end = NULL;
    unsigned long long v = strtoull(sb.items, &end, 10);
    bool ok = (end != sb.items);
    sb_free(sb);

    if (!ok) return false;
    *out = v;
    return true;
}

static bool schema_write_version(uint64_t v)
{
    char buf[32];
    int n = snprintf(buf, sizeof(buf), "%llu\n", (unsigned long long)v);
    if (n < 0) return false;
    return fs_write_file(TATR_VERSION_PATH, buf, (size_t)n);
}

bool schema_write_current(void)
{
    return schema_write_version(TATR_SCHEMA_VERSION);
}

/*
   migration table
  
   Add a new row (and bump TATR_SCHEMA_VERSION) whenever the on-disk
   repo/issue format changes. Each migrate_fn only needs to get the repo
   from exactly `from` to exactly `to`.
   schema_check_and_migrate() walks the chain and persists the version after 
   each successful step, so a repo several versions behind is migrated one
   step at a time.
*/

typedef Tatr_Error (*Schema_Migrate_Fn)(void);

// 0 -> 1: repos created before schema versioning existed.
// The on-disk issue format itself hasn't changed as part of this migration
// introducing the version marker *is* the migration. The only thing worth
// checking is that the layout version 1 assumes (.tatr/issues/) is actually there.
static Tatr_Error migrate_0_to_1(void)
{
    if (!fs_file_exists(TATR_ISSUES_PATH)) {
        log_error(".tatr/issues is missing. Repo may be corrupted");
        return TATR_ERR_STORAGE;
    }
    return TATR_OK;
}

typedef struct {
    uint64_t from;
    uint64_t to;
    Schema_Migrate_Fn migrate;
} Schema_Migration;

static const Schema_Migration MIGRATIONS[] = {
    { 0, 1, migrate_0_to_1 },
};
#define MIGRATIONS_COUNT (sizeof(MIGRATIONS) / sizeof(MIGRATIONS[0]))

Tatr_Error schema_check_and_migrate(void)
{
    uint64_t version;
    if (!schema_read_version(&version)) {
        log_error("cannot read .tatr/VERSION");
        return TATR_ERR_STORAGE;
    }

    if (version > TATR_SCHEMA_VERSION) {
        log_error(
            "this repo's schema version (%lu) is newer than this "
            "build of tatr understands (%d) -- upgrade tatr",
            version, TATR_SCHEMA_VERSION);
        return TATR_ERR_CONFLICT;
    }

    while (version < TATR_SCHEMA_VERSION) {
        const Schema_Migration *step = NULL;
        for (size_t i = 0; i < MIGRATIONS_COUNT; i++) {
            if (MIGRATIONS[i].from == version) {
                step = &MIGRATIONS[i];
                break;
            }
        }

        if (!step) {
            log_error("no migration path from schema version %llu to %d", (ULL)version, TATR_SCHEMA_VERSION);
            return TATR_ERR_CONFLICT;
        }

        Tatr_Error err = step->migrate();
        if (err != TATR_OK) return err;

        if (!schema_write_version(step->to))
            return TATR_ERR_IO;

        log_info("migrated repo schema %llu -> %llu", (ULL)step->from, (ULL)step->to);

        version = step->to;
    }

    return TATR_OK;
}

#undef ULL
