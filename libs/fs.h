#ifndef FS_H_
#define FS_H_

#include "astring.h"
#include <dirent.h>

#ifdef _WIN32
#include <windows.h>
#endif

typedef struct {
    const char **items;
    size_t count;
    size_t capacity;
} File_Paths;

typedef enum {
    FST_REGULAR = 0,
    FST_DIRECTORY,
    FST_SYMLINK,
    FST_OTHER,
} File_Type;

const char *fs__path(const char *first, ...);
#define fs_path(...) fs__path(__VA_ARGS__, NULL)

const char *fs_path_name(const char *path);
bool fs_file_exists(const char *file_path);
bool fs_unique_path(const char *dir, const char *filename, String_Builder *out);
bool fs_rename(const char *old_path, const char *new_path);
bool fs_mkdir(const char *path);
bool fs_copy_file(const char *src_path, const char *dst_path);
bool fs_read_dir(const char *parent, File_Paths *children);
bool fs_read_file(const char *path, String_Builder *sb);
bool fs_write_file(const char *path, const void *data, size_t size);
bool fs_delete_file(const char *path);
bool fs_delete_recursive(const char *path);
bool fs_append_file(const char *path, const char *data);

File_Type fs_get_file_type(const char *path);

typedef struct {
    char *name;
    bool error;

#ifdef _WIN32
    WIN32_FIND_DATA __win32_data;
    HANDLE __win32_hFind;
    bool __win32_init;
#else
    DIR *__posix_dir;
    struct dirent *__posix_ent;
#endif // _WIN32
} Dir_Entry;

bool fs_deopen(const char *dir_path, Dir_Entry *dir);
bool fs_denext(Dir_Entry *dir);
void fs_declose(Dir_Entry dir);

#endif // FS_H_
