#include "temp.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>

#ifndef TEMP_BLOCK_DEFAULT_SIZE
#define TEMP_BLOCK_DEFAULT_SIZE (64 * 1024)
#endif

#ifndef TEMP_ALIGNMENT
#define TEMP_ALIGNMENT (sizeof(void *))
#endif

struct Temp_Block {
    size_t capacity;
    size_t used;
    struct Temp_Block *next;
    char data[];
};

typedef struct {
    Temp_Block *head;
    Temp_Block *current;
} Temp_Arena;

static Temp_Arena arena = {0};

static size_t align_up(size_t x, size_t align)
{
    assert((align & (align - 1)) == 0 && "alignment must be power of two");
    return (x + align - 1) & ~(align - 1);
}

static Temp_Block *temp_block_create(size_t min_capacity)
{
    size_t cap = TEMP_BLOCK_DEFAULT_SIZE;
    if (cap < min_capacity) cap = min_capacity;

    Temp_Block *block = (Temp_Block *)malloc(sizeof(Temp_Block) + cap);
    assert(block && "temp: out of memory");

    block->capacity = cap;
    block->used = 0;
    block->next = NULL;

    return block;
}

void temp_init(void)
{
    arena.head = NULL;
    arena.current = NULL;
}

void temp_destroy(void)
{
    temp_reset();
}

void *temp_alloc(size_t size)
{
    return temp_alloc_aligned(size, TEMP_ALIGNMENT);
}

void *temp_alloc_aligned(size_t size, size_t alignment)
{
    if (size == 0) return NULL;

    if (!arena.current)
        arena.head = arena.current = temp_block_create(size + alignment);

    Temp_Block *b = arena.current;
    size_t offset = align_up(b->used, alignment);
    if (offset + size > b->capacity) {
        Temp_Block *new_block = temp_block_create(size + alignment);
        b->next = new_block;
        arena.current = new_block;
        b = new_block;

        offset = 0;
    }

    void *ptr = b->data + offset;
    b->used = offset + size;

    return ptr;
}

void temp_reset(void)
{
    Temp_Block *b = arena.head;
    while (b) {
        Temp_Block *next = b->next;
        free(b);
        b = next;
    }

    arena.head = NULL;
    arena.current = NULL;
}

Temp_Checkpoint temp_save(void)
{
    Temp_Checkpoint cp;
    cp.block = arena.current;
    cp.used  = arena.current ? arena.current->used : 0;
    return cp;
}

void temp_rewind(Temp_Checkpoint cp)
{
    if (!cp.block) {
        temp_reset();
        return;
    }

    Temp_Block *b = cp.block->next;
    while (b) {
        Temp_Block *next = b->next;
        free(b);
        b = next;
    }

    cp.block->next = NULL;
    cp.block->used = cp.used;

    arena.current = cp.block;
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
