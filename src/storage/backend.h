#ifndef TATR_STORAGE_BACKEND_H_
#define TATR_STORAGE_BACKEND_H_

#include "error.h"
#include "astring.h"
#include "fs.h"
#include <stdbool.h>
#include <stddef.h>

typedef struct Storage_Backend Storage_Backend;

typedef struct {
    // Load the raw bytes of the record for `id` into *out.
    // Caller owns *out (sb_free). TATR_ERR_NOT_FOUND if it doesn't exist.
    Tatr_Error (*load)(Storage_Backend *self, const char *id, String_Builder *out);

    // Persist `data`/`size` as the record for `id`, replacing it atomically.
    Tatr_Error (*save)(Storage_Backend *self, const char *id, const char *data, size_t size);

    // List all known record ids. Caller owns *out (da_free). Order is
    // backend-defined. Callers that need a specific order sort it themselves.
    Tatr_Error (*list)(Storage_Backend *self, File_Paths *out);

    // Permanently remove the record for `id`.
    Tatr_Error (*remove)(Storage_Backend *self, const char *id);

    // Whether a record for `id` currently exists.
    bool (*exists)(Storage_Backend *self, const char *id);

    // Reserve storage for a brand-new record `id`. Must succeed before
    // the first save() for that id. TATR_ERR_CONFLICT if `id` already exists.
    Tatr_Error (*create)(Storage_Backend *self, const char *id);

    // Backend-specific location for record-associated files that are not
    // part of the record itself (tatr's attachments/). Temp-allocated
    // NULL if the backend has no such concept.
    const char *(*attachment_dir)(Storage_Backend *self, const char *id);
} Storage_Backend_Vtable;

struct Storage_Backend {
    const Storage_Backend_Vtable *vt;
    void *ctx;
};

#define storage_load(be, id, out)        ((be)->vt->load((be), (id), (out)))
#define storage_save(be, id, data, size) ((be)->vt->save((be), (id), (data), (size)))
#define storage_list(be, out)            ((be)->vt->list((be), (out)))
#define storage_remove(be, id)           ((be)->vt->remove((be), (id)))
#define storage_exists(be, id)           ((be)->vt->exists((be), (id)))
#define storage_create(be, id)           ((be)->vt->create((be), (id)))
#define storage_attachment_dir(be, id)   ((be)->vt->attachment_dir((be), (id)))

Storage_Backend *storage_backend_flatfile(void);

#endif // TATR_STORAGE_BACKEND_H_
