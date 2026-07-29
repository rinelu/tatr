#ifndef TATR_SCHEMA_H_
#define TATR_SCHEMA_H_

/*
   Repo-level schema versioning.
  
   TATR_SCHEMA_VERSION is a plain integer describing the on-disk repo/issue
   format. ITS NOT tatr's own semver (TATR_VERSION in global.h). It's stored
   in .tatr/VERSION, written by `tatr init`, and checked on every command
   invocation via schema_check_and_migrate() (see require_repo() in
   commands/cmd.h).
  
   Migrations are forward-only, there is no documented downgrade path
   for a repo a newer tatr has already touched.
   A repo whose version is newer than TATR_SCHEMA_VERSION is refused.
*/

#include "error.h"
#include <stdint.h>
#include <stdbool.h>

#define TATR_SCHEMA_VERSION 1

bool schema_write_current(void);

// Read .tatr/VERSION and walk the migration table up to TATR_SCHEMA_VERSION,
// persisting the new version after each successful step and logging what happened.
//
//   TATR_OK           - already current, or migrated successfully
//   TATR_ERR_CONFLICT - repo's version is newer than this binary knows
//                       about, or there is no migration path from its
//                       current version
//   TATR_ERR_IO       - couldn't write .tatr/VERSION
//   TATR_ERR_STORAGE  - couldn't read .tatr/VERSION, or a migration step
//                       found the repo in an unexpected state
Tatr_Error schema_check_and_migrate(void);

#endif // TATR_SCHEMA_H_
