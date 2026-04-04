#include "fs.h"
#include "temp.h"

#include <assert.h>
#include <dirent.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stddef.h>
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define printf(...) {}

#ifdef _WIN32
#define FS_SEP "\\"
#else
#define FS_SEP "/"
#endif

const char *fs__path(const char *first, ...)
{
    String_Builder sb = {0};

    sb_append_cstr(&sb, first);

    va_list ap;
    va_start(ap, first);

    const char *part;
    while ((part = va_arg(ap, const char *))) {
        sb_append_cstr(&sb, FS_SEP);
        sb_append_cstr(&sb, part);
    }

    va_end(ap);
    sb_append_null(&sb);

    char *result = temp_strdup(sb.items);
    sb_free(sb);

    return result;
}

const char *fs_path_name(const char *path)
{
#ifdef _WIN32
    const char *p1 = strrchr(path, '/');
    const char *p2 = strrchr(path, '\\');
    const char *p = (p1 > p2)? p1 : p2;
    // NULL is ignored if the other search is successful 
    return p ? p + 1 : path;
#else
    const char *p = strrchr(path, '/');
    return p ? p + 1 : path;
#endif // _WIN32
}

bool fs_unique_path(const char *dir, const char *filename, String_Builder *out)
{
    String_Builder base = {0};
    String_Builder ext  = {0};

    const char *dot = strrchr(filename, '.');
    if (dot) {
        sb_append_buf(&base, filename, (unsigned long)(dot - filename));
        sb_append_cstr(&ext, dot);
    } else {
        sb_append_cstr(&base, filename);
    }

    // First try original name
    sb_append_cstr(out, fs_path(dir, filename));
    if (!fs_file_exists(out->items)) {
        sb_free(base);
        sb_free(ext);
        return true;
    }

    // Try numbered versions
    for (int i = 1; i < 10000; i++) {
        out->count = 0;

        sb_append_cstr(out, dir);
        sb_append_cstr(out, FS_SEP);

        sb_appendf(out, SV_Fmt"-%d"SV_Fmt, SB_Arg(base), i, SB_Arg(ext));

        if (!fs_file_exists(out->items)) {
            sb_free(base);
            sb_free(ext);
            return true;
        }
    }

    sb_free(base);
    sb_free(ext);
    return false;
}

// RETURNS:
//  0 - file doesn't exists
//  1 - file exists
bool fs_file_exists(const char *file_path)
{
#if _WIN32
    return GetFileAttributesA(file_path) != INVALID_FILE_ATTRIBUTES;
#else
    return access(file_path, F_OK) == 0;
#endif
}

bool fs_rename(const char *old_path, const char *new_path)
{
#ifdef _WIN32
    if (!MoveFileEx(old_path, new_path, MOVEFILE_REPLACE_EXISTING)) {
        printf("could not rename %s to %s: %s\n", old_path, new_path, NULL /* TODO: Write windows error code */);
        return false;
    }
#else
    if (rename(old_path, new_path) < 0) {
        printf("Could not rename %s to %s: %s\n", old_path, new_path, strerror(errno));
        return false;
    }
#endif // _WIN32
    return true;
}

bool fs_mkdir(const char *path)
{
#ifdef _WIN32
    int result = _mkdir(path);
#else
    int result = mkdir(path, 0755);
#endif
    if (result < 0) {
        if (errno == EEXIST) return false;
        printf("Could not create directory `%s`: %s\n", path, strerror(errno));
        return false;
    }
    return true;
}

bool fs_copy_file(const char *src_path, const char *dst_path)
{
#ifdef _WIN32
    if (!CopyFile(src_path, dst_path, FALSE)) {
        printf("Could not copy file: %s\n", NULL /* TODO: Write windows error code */);
        return false;
    }
    return true;
#else
    int fd = -1;
    int dst_fd = -1;
    size_t buf_size = 32*1024;
    char *buf = (char*)realloc(NULL, buf_size);
    assert(buf != NULL && "memory not enough");
    bool result = false;

    fd = open(src_path, O_RDONLY);
    if (fd < 0) {
        printf("Could not open file %s: %s\n", src_path, strerror(errno));
        goto defer;
    }

    struct stat stat;
    if (fstat(fd, &stat) < 0) {
        printf("Could not get mode of file %s: %s\n", src_path, strerror(errno));
        goto defer;
    }

    dst_fd = open(dst_path, O_CREAT | O_TRUNC | O_WRONLY, stat.st_mode);
    if (dst_fd < 0) {
        printf("Could not create file %s: %s\n", dst_path, strerror(errno));
        goto defer;
    }

    for (;;) {
        ssize_t n = read(fd, buf, buf_size);
        if (n == 0) break;
        if (n < 0) {
            printf("Could not read from file %s: %s", src_path, strerror(errno));
            goto defer;
        }

        char *buf2 = buf;
        while (n > 0) {
            ssize_t m = write(dst_fd, buf2, (size_t)n);
            if (m < 0) {
                printf("Could not write to file %s: %s\n", dst_path, strerror(errno));
                goto defer;
            }
            n    -= m;
            buf2 += m;
        }
    }

    result = true;
defer:
    free(buf);
    if (fd >= 0)     close(fd);
    if (dst_fd >= 0) close(dst_fd);
    return result;
#endif
}


bool fs_read_file(const char *path, String_Builder *sb)
{
    bool result = false;

    FILE *f = fopen(path, "rb");
    size_t new_count = 0;
    long long m = 0;
    if (f == NULL)                 goto defer;
    if (fseek(f, 0, SEEK_END) < 0) goto defer;
#ifndef _WIN32
    m = ftell(f);
#else
    long m = _telli64(_fileno(f));
#endif
    if (m < 0)                     goto defer;
    if (fseek(f, 0, SEEK_SET) < 0) goto defer;

    new_count = sb->count + (size_t)m;
    if (new_count > sb->capacity) {
        sb->items = DELCTYPE(sb->items)realloc(sb->items, new_count + 1);
        assert(sb->items != NULL && "memory not enough");
        sb->capacity = new_count;
    }

    size_t n = fread(sb->items + sb->count, 1, (size_t)m, f);
    if (n != (size_t)m) goto defer;
    if (ferror(f)) goto defer;

    sb->count = new_count;
    result = true;

defer:
    if (!result) printf("Could not read file %s: %s\n", path, strerror(errno));
    if (f) fclose(f);
    return result;
}

bool fs_read_dir(const char *parent, File_Paths *children)
{
    if (strlen(parent) == 0) {
        printf("Cannot read empty path\n");
        return false;
    }

    bool result = false;
    Dir_Entry dir = {0};

    if (!fs_deopen(parent, &dir)) goto defer;
    while (fs_denext(&dir)) {
        if (!strcmp(dir.name, ".") || !strcmp(dir.name, ".."))
            continue;
        da_append(children, temp_strdup(dir.name));
    }

    if (dir.error) goto defer;
    result = true;

defer:
    fs_declose(dir);
    return result;
}

bool fs_write_file(const char *path, const void *data, size_t size)
{
    bool result = false;

    const char *buf = NULL;
    FILE *f = fopen(path, "wb");
    if (f == NULL) {
        printf("Could not open file %s for writing: %s\n", path, strerror(errno));
        goto defer;
    }

    buf = (const char*)data;
    while (size > 0) {
        size_t n = fwrite(buf, 1, size, f);
        if (ferror(f)) {
            printf("Could not write into file %s: %s\n", path, strerror(errno));
            goto defer;
        }
        size -= n;
        buf  += n;
    }
    result = true;

defer:
    if (f) fclose(f);
    return result;
}

bool fs_delete_file(const char *path)
{
#ifdef _WIN32
    File_Type type = fs_get_file_type(path);
    switch (type) {
    case FST_DIRECTORY:
        if (!RemoveDirectoryA(path)) {
            printf("Could not delete directory %s: %s\n", path, NULL /* TODO: Write windows error code */);
            return false;
        }
        break;
    case FST_REGULAR:
    case FST_SYMLINK:
    case FST_OTHER:
        if (!DeleteFileA(path)) {
            printf("Could not delete file %s: %s\n", path, NULL /* TODO: Write windows error code */);
            return false;
        }
        break;
    default:
        printf("Unreachable: File_Type");
        abort();
    }
    return true;
#else
    if (remove(path) < 0) {
        printf("Could not delete file %s: %s\n", path, strerror(errno));
        return false;
    }
    return true;
#endif // _WIN32
}

bool fs_delete_recursive(const char *path)
{
    File_Type type = fs_get_file_type(path);
    Dir_Entry dir = {0};
    bool result = false;
    if ((int)type < 0) goto defer;

    if (type == FST_DIRECTORY) {
        if (!fs_deopen(path, &dir)) return false;

        while (fs_denext(&dir)) {
            if (!strcmp(dir.name, ".") || !strcmp(dir.name, ".."))
                continue;

            const char *child = fs_path(path, dir.name);

            if (!fs_delete_recursive(child)) {
                free((void*)child);
                goto defer;
            }

            free((void*)child);
        }

        if (dir.error) goto defer;

#ifdef _WIN32
        if (!RemoveDirectoryA(path)) {
            printf("Could not delete directory %s\n", path);
            goto defer;
        }
#else
        if (rmdir(path) < 0) {
            printf("Could not delete directory %s: %s\n", path, strerror(errno));
            goto defer;
        }
#endif
        result = true;
        goto defer;
    }

#ifdef _WIN32
    if (!DeleteFileA(path)) {
        printf("Could not delete file %s\n", path);
        goto defer;
    }
#else
    if (unlink(path) < 0) {
        printf("Could not delete file %s: %s\n", path, strerror(errno));
        goto defer;
    }
#endif

    result = true;
defer:
    fs_declose(dir);
    return result;
}

bool fs_append_file(const char *path, const char *data)
{
    FILE *f = fopen(path, "ab");
    if (!f) return false;

    size_t len = strlen(data);
    bool ok = fwrite(data, 1, len, f) == len;

    fclose(f);
    return ok;
}

File_Type fs_get_file_type(const char *path)
{
#ifdef _WIN32
    DWORD attr = GetFileAttributesA(path);
    if (attr == INVALID_FILE_ATTRIBUTES) {
        printf("Could not get file attributes of %s: %s", path, NULL /* TODO: Write windows error code */);
        return -1;
    }

    if (attr & FILE_ATTRIBUTE_DIRECTORY) return FST_DIRECTORY;
    return FST_REGULAR;
#else // _WIN32
    struct stat statbuf;
    if (lstat(path, &statbuf) < 0) {
        printf("Could not get stat of %s: %s\n", path, strerror(errno));
        return (File_Type)(-1);
    }

    if (S_ISREG(statbuf.st_mode)) return FST_REGULAR;
    if (S_ISDIR(statbuf.st_mode)) return FST_DIRECTORY;
    if (S_ISLNK(statbuf.st_mode)) return FST_SYMLINK;
    return FST_OTHER;
#endif // _WIN32
}

bool fs_deopen(const char *dir_path, Dir_Entry *dir)
{
    memset(dir, 0, sizeof(*dir));
#ifdef _WIN32
    memset(dir, 0, sizeof(*dir));

    size_t len = strlen(dir_path);
    char *pattern = (char*)malloc(len + 3); // "\*" + null
    if (!pattern) {
        dir->error = true;
        return false;
    }

    memcpy(pattern, dir_path, len);
    pattern[len] = '\\';
    pattern[len + 1] = '*';
    pattern[len + 2] = '\0';

    dir->__win32_hFind = FindFirstFileA(pattern, &dir->__win32_data);
    free(pattern);

    if (dir->__win32_hFind == INVALID_HANDLE_VALUE) {
        printf("Could not open directory %s\n", dir_path);
        dir->error = true;
        return false;
    }

    dir->__win32_init = false;
#else
    dir->__posix_dir = opendir(dir_path);
    if (dir->__posix_dir == NULL) {
        printf("Could not open directory %s: %s\n", dir_path, strerror(errno));
        dir->error = true;
        return false;
    }
#endif // _WIN32
    return true;
}

bool fs_denext(Dir_Entry *dir)
{
#ifdef _WIN32
    if (!dir->__win32_init) {
        dir->__win32_init = true;
        dir->name = dir->__win32_data.cFileName;
        return true;
    }

    if (!FindNextFile(dir->__win32_hFind, &dir->__win32_data)) {
        if (GetLastError() == ERROR_NO_MORE_FILES) return false;
        printf("Could not read next directory entry: %s", NULL /* TODO: Write windows error code */);
        dir->error = true;
        return false;
    }
    dir->name = dir->__win32_data.cFileName;
#else
    errno = 0;
    dir->__posix_ent = readdir(dir->__posix_dir);
    if (dir->__posix_ent == NULL) {
        if (errno == 0) return false;
        printf("Could not read next directory entry: %s\n", strerror(errno));
        dir->error = true;
        return false;
    }
    dir->name = dir->__posix_ent->d_name;
#endif // _WIN32
    return true;
}

void fs_declose(Dir_Entry dir)
{
#ifdef _WIN32
    FindClose(dir.__win32_hFind);
#else
    if (dir.__posix_dir) closedir(dir.__posix_dir);
#endif
}
