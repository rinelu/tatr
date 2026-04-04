#include "astring.h"
#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>

int sb_appendf(String_Builder *sb, const char *fmt, ...)
{
    va_list args;

    va_start(args, fmt);
    int n = vsnprintf(NULL, 0, fmt, args);
    va_end(args);

    da_reserve(sb, sb->count + (size_t)n + 1);
    char *dest = sb->items + sb->count;
    va_start(args, fmt);
    vsnprintf(dest, (size_t)n + 1, fmt, args);
    va_end(args);

    sb->count += (size_t)n;

    return n;
}

void sb_pad_align(String_Builder *sb, size_t size)
{
    size_t rem = sb->count % size;
    if (rem == 0) return;
    for (size_t i = 0; i < size - rem; ++i)
        da_append(sb, 0);
}

// --------------------------

String_View sv_slice_while(String_View *sv, int (*p)(int x))
{
    size_t i = 0;
    while (i < sv->count && p(sv->data[i])) i += 1;

    String_View result = sv_from_parts(sv->data, i);
    sv->count -= i;
    sv->data  += i;

    return result;
}

String_View sv_slice_by_delim(String_View *sv, char delim)
{
    size_t i = 0;
    while (i < sv->count && sv->data[i] != delim) i += 1;

    String_View result = sv_from_parts(sv->data, i);

    if (i < sv->count) {
        sv->count -= i + 1;
        sv->data  += i + 1;
    } else {
        sv->count -= i;
        sv->data  += i;
    }

    return result;
}

bool sv_slice_prefix(String_View *sv, String_View prefix)
{
    if (sv_starts_with(*sv, prefix)) {
        sv_slice_left(sv, prefix.count);
        return true;
    }
    return false;
}

bool sv_slice_suffix(String_View *sv, String_View suffix)
{
    if (sv_ends_with(*sv, suffix)) {
        sv_slice_right(sv, suffix.count);
        return true;
    }
    return false;
}

String_View sv_slice_left(String_View *sv, size_t n)
{
    if (n > sv->count) n = sv->count;

    String_View result = sv_from_parts(sv->data, n);
    sv->data  += n;
    sv->count -= n;
    return result;
}

String_View sv_slice_right(String_View *sv, size_t n)
{
    if (n > sv->count) n = sv->count;

    String_View result = sv_from_parts(sv->data + sv->count - n, n);
    sv->count -= n;
    return result;
}

String_View sv_from_parts(const char *data, size_t count)
{
    String_View sv;
    sv.count = count;
    sv.data = data;
    return sv;
}

String_View sv_trim_left(String_View sv)
{
    size_t i = 0;
    while (i < sv.count && isspace(sv.data[i])) i += 1;

    return sv_from_parts(sv.data + i, sv.count - i);
}

String_View sv_trim_right(String_View sv)
{
    size_t i = 0;
    while (i < sv.count && isspace(sv.data[sv.count - 1 - i])) i += 1;

    return sv_from_parts(sv.data, sv.count - i);
}

String_View sv_trim(String_View sv)
{
    return sv_trim_right(sv_trim_left(sv));
}

String_View sv_from_cstr(const char *cstr)
{
    return sv_from_parts(cstr, strlen(cstr));
}

bool sv_eq(String_View a, String_View b)
{
    if (a.count != b.count) return false;

    return memcmp(a.data, b.data, a.count) == 0;
}

bool sv_eq_cstr(String_View a, const char* b)
{
    return sv_eq(a, sv_from_cstr(b));
}

bool sv_has(String_View sv, const char *key, const char *delim)
{
    String_View cursor = sv;

    while (cursor.count > 0) {
        String_View part = sv_trim(sv_slice_by_delim(&cursor, *delim));
        if (sv_eq(part, sv_from_cstr(key))) return true;
    }

    return false;
}

bool sv_contains(String_View haystack, String_View needle)
{
    if (needle.count == 0) return true;
    if (needle.count > haystack.count) return false;

    String_View cursor = haystack;

    while (cursor.count >= needle.count) {
        if (sv_starts_with(cursor, needle)) 
            return true;
        sv_slice_left(&cursor, 1);
    }

    return false;
}

bool sv_icontains(String_View haystack, String_View needle)
{
    if (needle.count == 0) return true;
    if (needle.count > haystack.count) return false;

    String_View cursor = haystack;

    while (cursor.count >= needle.count) {
        String_View prefix = sv_from_parts(cursor.data, needle.count);

        size_t i = 0;
        for (; i < needle.count; i++) {
            char a = prefix.data[i];
            char b = needle.data[i];

            if (tolower((unsigned char)a) != tolower((unsigned char)b))
                break;
        }

        if (i == needle.count)
            return true;

        sv_slice_left(&cursor, 1);
    }

    return false;
}

bool sv_end_with(String_View sv, const char *cstr)
{
    return sv_ends_with_cstr(sv, cstr);
}

bool sv_ends_with_cstr(String_View sv, const char *cstr)
{
    return sv_ends_with(sv, sv_from_cstr(cstr));
}

bool sv_ends_with(String_View sv, String_View suffix)
{
    if (sv.count >= suffix.count) {
        String_View sv_tail = {
            .count = suffix.count,
            .data = sv.data + sv.count - suffix.count,
        };
        return sv_eq(sv_tail, suffix);
    }
    return false;
}

bool sv_starts_with(String_View sv, String_View expected_prefix)
{
    if (expected_prefix.count <= sv.count) {
        String_View actual_prefix = sv_from_parts(sv.data, expected_prefix.count);
        return sv_eq(expected_prefix, actual_prefix);
    }

    return false;
}

char *sv_dup(String_View sv)
{
    char *s = malloc(sv.count + 1);
    memcpy(s, sv.data, sv.count);
    s[sv.count] = '\0';
    return s;
}
