#include "global.h"

#include "temp.h"
#include "fs.h"

const char *tatr_path(const char *rel)
{
    if (g_repo_root[0] == '\0') return rel;
    return temp_sprintf("%s" FS_SEP "%s", g_repo_root, rel);
}
