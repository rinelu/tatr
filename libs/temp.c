#include "temp.h"

#include <string.h>
#include <stdio.h>
#include <assert.h>

// TODO: use proper arena

static size_t temp_size = 0;
static char temp[TEMP_CAPACITY] = {0};

void *temp_alloc(size_t size)
{
    assert(temp_size + size <= TEMP_CAPACITY && "temp: not enough memory");

    void *ptr = temp + temp_size;
    temp_size += size;
    return ptr;
}

void temp_reset(void)
{
    temp_size = 0;
}

size_t temp_save(void)
{
    return temp_size;
}

void temp_rewind(size_t checkpoint)
{
    assert(checkpoint <= temp_size);
    temp_size = checkpoint;
}

char *temp_strdup(const char *cstr)
{
    size_t len = strlen(cstr) + 1;
    char *dst = temp_alloc(len);
    if (!dst) return NULL;

    memcpy(dst, cstr, len);
    return dst;
}

char *temp_strndup(const char *cstr, size_t size)
{
    char *dst = temp_alloc(size + 1);
    if (!dst) return NULL;

    memcpy(dst, cstr, size);
    dst[size] = '\0';
    return dst;
}

char *temp_vsprintf(const char *format, va_list ap)
{
    va_list ap_copy;
    va_copy(ap_copy, ap);

    int needed = vsnprintf(NULL, 0, format, ap_copy);
    va_end(ap_copy);

    if (needed < 0) return NULL;

    char *buf = temp_alloc((size_t)needed + 1);
    if (!buf) return NULL;

    vsnprintf(buf, (size_t)needed + 1, format, ap);
    return buf;
}

char *temp_sprintf(const char *format, ...)
{
    va_list ap;
    va_start(ap, format);
    char *result = temp_vsprintf(format, ap);
    va_end(ap);
    return result;
}

const char *temp_sv_to_cstr(String_View sv)
{
    char *buf = temp_alloc(sv.count + 1);
    if (!buf) return NULL;

    memcpy(buf, sv.data, sv.count);
    buf[sv.count] = '\0';
    return buf;
}
