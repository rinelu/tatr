#ifndef TEMP_H
#define TEMP_H

#include "astring.h"
#include "util.h"
#include <stddef.h>
#include <stdarg.h>

#ifndef TEMP_CAPACITY
#define TEMP_CAPACITY (8*1024*1024)
#endif // TEMP_CAPACITY

void *temp_alloc(size_t size);
void temp_reset(void);
size_t temp_save(void);
void temp_rewind(size_t checkpoint);

char *temp_strdup(const char *cstr);
char *temp_strndup(const char *cstr, size_t size);
char *temp_sprintf(const char *format, ...) FMT(1, 2);
char *temp_vsprintf(const char *format, va_list ap);

const char *temp_sv_to_cstr(String_View sv);

#endif // TEMP_H
