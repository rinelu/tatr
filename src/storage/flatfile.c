#include "backend.h"

#include "fs.h"
#include "global.h"
#include "temp.h"

#include <string.h>

// On-disk layout:
//   .tatr/issues/<id>/issue.tatr
//   .tatr/issues/<id>/attachments/
// Relocated here verbatim from core/issue.c so it lives behind the
// interface instead of being assumed inside the issue model.

static bool flatfile__id_is_safe(const char *id)
{
    if (!id || !*id)                   return false;
    if (strcmp(id, ".")  == 0) return false;
    if (strcmp(id, "..") == 0) return false;

    for (const char *p = id; *p; p++) {
        if (*p == '/' || *p == '\\')  return false;
        if ((unsigned char)*p < 0x20) return false; // control chars
    }
    return true;
}

static const char *flatfile__dir(const char *id)
{
    return fs_path(TATR_ISSUES_PATH, id);
}

static const char *flatfile__file(const char *id)
{
    return fs_path(flatfile__dir(id), "issue.tatr");
}

static Tatr_Error flatfile_load(Storage_Backend *self, const char *id, String_Builder *out)
{
    (void)self;
    if (!flatfile__id_is_safe(id)) return TATR_ERR_INVALID_ARG;

    if (!fs_read_file(flatfile__file(id), out))
        return TATR_ERR_NOT_FOUND;

    return TATR_OK;
}

static Tatr_Error flatfile_save(Storage_Backend *self, const char *id, const char *data, size_t size)
{
    (void)self;
    if (!flatfile__id_is_safe(id)) return TATR_ERR_INVALID_ARG;

    Temp_Checkpoint tmark = temp_save();
    const char *path = flatfile__file(id);

    String_Builder tmp_path = {0};
    sb_appendf(&tmp_path, "%s.tmp", path);
    sb_append_null(&tmp_path);

    Tatr_Error result = TATR_ERR_IO;
    if (!fs_write_file(tmp_path.items, data, size))     goto defer;
    if (!fs_rename(tmp_path.items, path)) goto defer;

    result = TATR_OK;
defer:
    sb_free(tmp_path);
    temp_rewind(tmark);
    return result;
}

static Tatr_Error flatfile_list(Storage_Backend *self, File_Paths *out)
{
    (void)self;
    if (!fs_read_dir(TATR_ISSUES_PATH, out))
        return TATR_ERR_STORAGE;
    return TATR_OK;
}

static Tatr_Error flatfile_remove(Storage_Backend *self, const char *id)
{
    (void)self;
    if (!flatfile__id_is_safe(id)) return TATR_ERR_INVALID_ARG;

    if (!fs_delete_recursive(flatfile__dir(id)))
        return TATR_ERR_IO;
    return TATR_OK;
}

static bool flatfile_exists(Storage_Backend *self, const char *id)
{
    (void)self;
    if (!flatfile__id_is_safe(id)) return false;
    return fs_file_exists(flatfile__file(id));
}

static Tatr_Error flatfile_create(Storage_Backend *self, const char *id)
{
    (void)self;
    if (!flatfile__id_is_safe(id)) return TATR_ERR_INVALID_ARG;

    const char *dir = flatfile__dir(id);
    if (fs_file_exists(dir)) return TATR_ERR_CONFLICT;

    if (!fs_mkdir(dir)) return TATR_ERR_IO;
    fs_mkdir(fs_path(dir, "attachments")); // best-effort, as before

    return TATR_OK;
}

static const char *flatfile_attachment_dir(Storage_Backend *self, const char *id)
{
    (void)self;
    if (!flatfile__id_is_safe(id)) return NULL;
    return fs_path(flatfile__dir(id), "attachments");
}

static const Storage_Backend_Vtable FLATFILE_VTABLE = {
    .load           = flatfile_load,
    .save           = flatfile_save,
    .list           = flatfile_list,
    .remove         = flatfile_remove,
    .exists         = flatfile_exists,
    .create         = flatfile_create,
    .attachment_dir = flatfile_attachment_dir,
};

Storage_Backend *storage_backend_flatfile(void)
{
    static Storage_Backend backend = { .vt = &FLATFILE_VTABLE, .ctx = NULL };
    return &backend;
}
