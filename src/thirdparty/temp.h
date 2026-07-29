#ifndef TEMP_H
#define TEMP_H

#include "astring.h"
#include "util.h"

#include <stddef.h>
#include <stdarg.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct Temp_Block Temp_Block;

typedef struct {
    Temp_Block *block;
    size_t used;
} Temp_Checkpoint;

void  temp_init(void);
void  temp_destroy(void);

void *temp_alloc(size_t size);
void *temp_alloc_aligned(size_t size, size_t alignment);

void  temp_reset(void);

Temp_Checkpoint temp_save(void);
void            temp_rewind(Temp_Checkpoint checkpoint);

char *temp_strdup(const char *cstr);
char *temp_strndup(const char *cstr, size_t size);

char *temp_sprintf(const char *format, ...) FMT(1, 2);
char *temp_vsprintf(const char *format, va_list ap);

const char *temp_sv_to_cstr(String_View sv);

#ifdef __cplusplus
}
#endif

#endif // TEMP_H
