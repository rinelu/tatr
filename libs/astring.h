#ifndef ASTRING_H_
#define ASTRING_H_

#include <stddef.h>
#include <stdbool.h>
#include "util.h"
#include "array.h"

typedef struct {
    char *items;
    size_t count;
    size_t capacity;
} String_Builder;

int sb_appendf(String_Builder *sb, const char *fmt, ...) FMT(2, 3);
void sb_pad_align(String_Builder *sb, size_t size);

#define sb_append_buf(sb, buf, size) da_appends(sb, buf, size)
#define sb_append_sv(sb, sv) sb_append_buf((sb), (sv).data, (sv).count)
#define sb_append_cstr(sb, cstr)      \
    do {                              \
        const char *s = (cstr);       \
        da_appends(sb, s, strlen(s)); \
    } while (0)

#define sb_append_null(sb) da_appends(sb, "", 1)
#define sb_append da_append
#define sb_free(sb) free((sb).items)

typedef struct {
    size_t count;
    const char *data;
} String_View;

#define sv_split_by_delim(sv, delim, out)                     \
    do {                                                      \
        while (sv.count > 0) {                                \
            String_View part = sv_slice_by_delim(&sv, delim); \
            da_append(out, part);                             \
        }                                                     \
    } while (0);

const char *temp_sv_to_cstr(String_View sv);

String_View sv_slice_while(String_View *sv, int (*p)(int x));
String_View sv_slice_by_delim(String_View *sv, char delim);
String_View sv_slice_left(String_View *sv, size_t n);
String_View sv_slice_right(String_View *sv, size_t n);
bool sv_slice_prefix(String_View *sv, String_View prefix);
bool sv_slice_suffix(String_View *sv, String_View suffix);

String_View sv_trim(String_View sv);
String_View sv_trim_left(String_View sv);
String_View sv_trim_right(String_View sv);
bool sv_eq(String_View a, String_View b);
bool sv_eq_cstr(String_View a, const char* b);
bool sv_has(String_View sv, const char *key, const char *delim);
bool sv_contains(String_View haystack, String_View needle);
bool sv_icontains(String_View haystack, String_View needle);
bool sv_ends_with_cstr(String_View sv, const char *cstr);
bool sv_ends_with(String_View sv, String_View suffix);
bool sv_starts_with(String_View sv, String_View prefix);
String_View sv_from_cstr(const char *cstr);
String_View sv_from_parts(const char *data, size_t count);
char *sv_dup(String_View sv);

#define sb_to_sv(sb) sv_from_parts((sb).items, (sb).count)
#define SV_Fmt "%.*s"
#define SV_Arg(sv) (int) (sv).count, (sv).data
#define SB_Arg(sv) (int) (sv).count, (sv).items

#endif // ASTRING_H_
