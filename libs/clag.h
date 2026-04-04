/*
   clag - v2.0.0 - Public Domain - Single-header CLI parser

   A tiny argument parsing library for C.

   # Quick Example

      ```c
      #define CLAG_IMPLEMENTATION
      #include "clag.h"

      int main(int argc, char **argv)
      {
          bool *verbose = clag_bool("verbose", false, "enable logging");
          uint64_t *count = clag_uint64("count", 10, "iteration count");

          if (!clag_parse(argc, argv)) {
              clag_print_error(stderr);
              clag_print_options(stderr);
              return 1;
          }

          printf("verbose: %s\n", *verbose ? "true" : "false");
          printf("count: %" PRIu64 "\n", *count);
          return 0;
      }
      ```

   # Parsing Rules

      - "--" stops flag parsing
      - First non-flag argument stops parsing
      - Remaining args available via:
            clag_rest_argc()
            clag_rest_argv()

   # Error Handling

      clag_parse() returns false on error.

      Use:
            clag_print_error(stderr);

      Possible errors:
            unknown flag
            missing value
            invalid number
            overflow
            invalid bool
            invalid size suffix

   # Configuration

      Optional macros:

            CLAG_CAP
                  Maximum number of flags (default: 256)

            CLAG_LIST_INIT_CAP
                  Initial list capacity (default: 8)
*/

#ifndef CLAG_H_
#define CLAG_H_

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <inttypes.h>
#include <limits.h>
#include <string.h>
#include <errno.h>
#include <float.h>

#ifndef CLAG_CAP
#define CLAG_CAP 256
#endif // CLAG_CAP

#ifndef CLAG_LIST_INIT_CAP
#define CLAG_LIST_INIT_CAP 8
#endif // CLAG_LIST_INIT_CAP

#ifndef CLAG_HELP_WIDTH
#define CLAG_HELP_WIDTH 80
#endif // CLAG_HELP_WIDTH

typedef struct {
    char **items;
    size_t count;
    size_t capacity;
} ClagList;

#define clag_list_append(lst, item)                                                \
    do {                                                                           \
        if ((lst)->count >= (lst)->capacity) {                                     \
            size_t _nc = (lst)->capacity == 0                                      \
                             ? CLAG_LIST_INIT_CAP : (lst)->capacity * 2;           \
            void *_p = realloc((lst)->items, _nc * sizeof(*(lst)->items));         \
            assert(_p && "clag: out of memory");                                   \
            (lst)->items    = (char **)_p;                                         \
            (lst)->capacity = _nc;                                                 \
        }                                                                          \
        (lst)->items[(lst)->count++] = (item);                                     \
    } while (0)

#define clag_list_free(lst)                          \
    do { free((lst)->items);                         \
         (lst)->items = NULL;                        \
         (lst)->count = (lst)->capacity = 0; } while(0)

// Register a flag and return a pointer to its storage.
// sc (short_char): pass 0 for no short form.
bool     *clag_bool  (const char *name, char sc, bool        def, const char *desc);
int64_t  *clag_int64 (const char *name, char sc, int64_t     def, const char *desc);
uint64_t *clag_uint64(const char *name, char sc, uint64_t    def, const char *desc);
float    *clag_float (const char *name, char sc, float       def, const char *desc);
double   *clag_double(const char *name, char sc, double      def, const char *desc);
size_t   *clag_size  (const char *name, char sc, const char *def, const char *desc);
char    **clag_str   (const char *name, char sc, const char *def, const char *desc);
// delim: character splitting repeated values (0 = no splitting, ',' is typical).
ClagList *clag_list  (const char *name, char sc, char        delim, const char *desc);

// Register a flag and store results into a caller-provided variable.
void clag_bool_var  (bool     *v, const char *name, char sc, bool        def, const char *desc);
void clag_int64_var (int64_t  *v, const char *name, char sc, int64_t     def, const char *desc);
void clag_uint64_var(uint64_t *v, const char *name, char sc, uint64_t    def, const char *desc);
void clag_float_var (float    *v, const char *name, char sc, float       def, const char *desc);
void clag_double_var(double   *v, const char *name, char sc, double      def, const char *desc);
void clag_size_var  (size_t   *v, const char *name, char sc, const char *def, const char *desc);
void clag_str_var   (char    **v, const char *name, char sc, const char *def, const char *desc);
void clag_list_var  (ClagList *v, const char *name, char sc, char        delim, const char *desc);

// Flag modifiers
void clag_required  (const char *name);                           // fail if not provided
void clag_deprecated(const char *name, const char *omessage);     // warn on use
void clag_hidden    (const char *name);                           // hide from --help
void clag_usage     (const char *synopsis);                       // synopsis for help header

// Parse argc/argv.  Returns true on success; false on the first error.
// On error, use clag_print_error() for a human-readable message.
// Automatically handles --help / -h.
bool clag_parse(int argc, char **argv);
 
// Arguments that were not consumed as flags
// (everything after "--" or first non-flag argument).
int    clag_rest_argc(void);
char **clag_rest_argv(void);
 
// argv[0] as supplied to clag_parse.
const char *clag_program_name(void);

void clag_print_error  (FILE *stream);
void clag_print_help   (FILE *stream);
void clag_print_options(FILE *stream);
 
// Lookup the registered name for an arbitrary value pointer
// (useful for custom error messages). Return NULL if not found.
const char *clag_name(void *val);

// True if the named flag was explicitly supplied.
bool clag_is_set(const char *name);

#endif // CLAG_H_

#ifdef CLAG_IMPLEMENTATION

typedef enum {
    CLAG_TYPE_BOOL = 0,
    CLAG_TYPE_INT64,
    CLAG_TYPE_UINT64,
    CLAG_TYPE_DOUBLE,
    CLAG_TYPE_FLOAT,
    CLAG_TYPE_SIZE,
    CLAG_TYPE_STR,
    CLAG_TYPE_LIST,
    COUNT_CLAG_TYPES,
} ClagType;

static_assert(COUNT_CLAG_TYPES == 8, "clag__apply/clag__type_name/clag__print_default is old");

typedef union {
    char     *as_str;
    int64_t  as_int64;
    uint64_t as_uint64;
    double   as_double;
    float    as_float;
    bool     as_bool;
    size_t   as_size;
    ClagList as_list;
} ClagValue;

typedef enum {
    CLAG_OK = 0,
    CLAG_ERR_UNKNOWN_FLAG,
    CLAG_ERR_NO_VALUE,
    CLAG_ERR_INVALID_NUMBER,
    CLAG_ERR_INT_OVERFLOW,
    CLAG_ERR_INT_UNDERFLOW,
    CLAG_ERR_FLOAT_OVERFLOW,
    CLAG_ERR_DOUBLE_OVERFLOW,
    CLAG_ERR_INVALID_SIZE_SUFFIX,
    CLAG_ERR_INVALID_BOOL,
    CLAG_ERR_REQUIRED,
    COUNT_CLAG_ERRORS,
} ClagError;

typedef struct {
    ClagType    type;
    const char *name;
    char        short_char;
    const char *desc;

    ClagValue  val;
    void      *ref;
    bool       ref_is_external; 

    ClagValue   def;
    // raw string for size default display
    const char *def_str;

    char           list_delim;

    // modifiers
    bool           required;
    bool           hidden;
    bool           deprecated;
    const char    *depr_msg;

    bool           is_set;
} Clag;

typedef struct {
    Clag   flags[CLAG_CAP];
    size_t flags_count;

    ClagError   error;
    const char *error_flag_name;
    const char *error_detail;

    const char *program_name;
    const char *usage_synopsis;

    int    rest_argc;
    char **rest_argv;
} ClagContext;

static ClagContext g_clag;

// ---------------
// Private helpers
// ---------------

static Clag *clag__alloc(ClagType type, const char *name, char sc, const char *desc)
{
    assert(g_clag.flags_count < CLAG_CAP && "CLAG_CAP exceeded; raise #define CLAG_CAP");
    for (size_t i = 0; i < g_clag.flags_count; i++) {
        assert(strcmp(g_clag.flags[i].name, name) != 0   && "clag: duplicate flag name");
        assert(!(sc && g_clag.flags[i].short_char == sc) && "clag: duplicate short flag character");
    }
    Clag *f = &g_clag.flags[g_clag.flags_count++];
    memset(f, 0, sizeof *f);
    f->type       = type;
    f->name       = name;
    f->short_char = sc;
    f->desc       = desc;
    return f;
}

static Clag *clag__find_long(const char *name)
{
    for (size_t i = 0; i < g_clag.flags_count; i++)
        if (strcmp(g_clag.flags[i].name, name) == 0)
            return &g_clag.flags[i];
    return NULL;
}

static Clag *clag__find_short(char sc)
{
    for (size_t i = 0; i < g_clag.flags_count; i++)
        if (g_clag.flags[i].short_char == sc)
            return &g_clag.flags[i];
    return NULL;
}

// Return a pointer to the relevant field inside ClagValue.
static void *clag__val_ptr(ClagValue *v, ClagType t)
{
    switch (t) {
        case CLAG_TYPE_BOOL:   return &v->as_bool;
        case CLAG_TYPE_INT64:  return &v->as_int64;
        case CLAG_TYPE_UINT64: return &v->as_uint64;
        case CLAG_TYPE_DOUBLE: return &v->as_double;
        case CLAG_TYPE_FLOAT:  return &v->as_float;
        case CLAG_TYPE_SIZE:   return &v->as_size;
        case CLAG_TYPE_STR:    return &v->as_str;
        case CLAG_TYPE_LIST:   return &v->as_list;
        default: assert(0 && "unreachable"); return NULL;
    }
}

// Parse optional size suffix. Multiplier is set on success.
// Returns false (and sets g_clag.error) on unrecognised suffix.
static bool clag__size_suffix(const char *endptr, unsigned long long *out_mult)
{
    *out_mult = 1;
    if (*endptr == '\0') return true;
 
    // Allow optional 'i' after the letter (Ki, Mi, …) and optional 'B' at end.
    char letter = *endptr;
    // ASCII upper
    if (letter >= 'a' && letter <= 'z') letter = (char)(letter - 32);
 
    unsigned long long m;
    switch (letter) {
        case 'K': m = 1ULL << 10; break;
        case 'M': m = 1ULL << 20; break;
        case 'G': m = 1ULL << 30; break;
        case 'T': m = 1ULL << 40; break;
        case 'P': m = 1ULL << 50; break;
        default:
            g_clag.error = CLAG_ERR_INVALID_SIZE_SUFFIX;
            return false;
    }
    endptr++;
    if (*endptr == 'i') endptr++;
    if (*endptr == 'B' || *endptr == 'b') endptr++;
    if (*endptr != '\0') {
        g_clag.error = CLAG_ERR_INVALID_SIZE_SUFFIX;
        return false;
    }
    *out_mult = m;
    return true;
}

static bool clag__parse_size_str(const char *raw, size_t *out)
{
#define RET_ERR(kind) { g_clag.error = kind; return false; }
    errno = 0;
    char *end;
    unsigned long long v = strtoull(raw, &end, 0);
    if (end == raw)      RET_ERR(CLAG_ERR_INVALID_NUMBER);
    if (errno == ERANGE) RET_ERR(CLAG_ERR_INT_OVERFLOW);

    unsigned long long m;
    if (!clag__size_suffix(end, &m)) return false;
    if (m > 1 && v > (unsigned long long)SIZE_MAX / m)
        RET_ERR(CLAG_ERR_INT_OVERFLOW);

    *out = (size_t)(v * m);
    return true;
#undef RET_ERR
}

// Parse a boolean string.
// Accepts: 1/0, true/false, yes/no, on/off (case-insensitive).
static bool clag__parse_bool(const char *s, bool *out)
{
#define RET_TRUE  { *out=true; return true; }
#define RET_FALSE { *out=false; return true; }
    // Fast path for single character
    if (s[1] == '\0') {
        if (*s == '1') RET_TRUE
        if (*s == '0') RET_FALSE
    }

    char buf[8];
    size_t len = strlen(s);
    if (len >= sizeof buf) goto fail;
    for (size_t i = 0; i <= len; i++)
        buf[i] = (s[i] >= 'A' && s[i] <= 'Z') ? (char)(s[i] + 32) : s[i];
 
    if (strcmp(buf, "true")  == 0 || strcmp(buf, "yes") == 0 || strcmp(buf, "on")  == 0) RET_TRUE
    if (strcmp(buf, "false") == 0 || strcmp(buf, "no")  == 0 || strcmp(buf, "off") == 0) RET_FALSE
fail:
    g_clag.error = CLAG_ERR_INVALID_BOOL;
    return false;
#undef RET_FALSE
#undef RET_TRUE
}

// Apply a parsed string value to a flag.
// Returns false on parse error.
static bool clag__apply_one(Clag *f, const char *raw)
{
#define RET_ERR(kind) { g_clag.error = kind; return false; }
    errno = 0;
    char *end;
 
    switch (f->type) {
 
    case CLAG_TYPE_BOOL: {
        bool v;
        if (!clag__parse_bool(raw, &v)) return false;
        *(bool *)f->ref = v;
        return true;
    }
    
    case CLAG_TYPE_INT64: {
        long long v = strtoll(raw, &end, 0);
        if (end == raw || *end != '\0') RET_ERR(CLAG_ERR_INVALID_NUMBER);
        if (errno == ERANGE) {
            g_clag.error = (v < 0) ? CLAG_ERR_INT_UNDERFLOW : CLAG_ERR_INT_OVERFLOW;
            return false;
        }
        *(int64_t *)f->ref = (int64_t)v;
        break;
    }

    case CLAG_TYPE_UINT64: {
        unsigned long long v = strtoull(raw, &end, 0);
        if (end == raw || *end != '\0')        RET_ERR(CLAG_ERR_INVALID_NUMBER);
        if (errno == ERANGE || v > UINT64_MAX) RET_ERR(CLAG_ERR_INT_OVERFLOW);

        *(uint64_t *)f->ref = (uint64_t)v;
        return true;
    }
 
    case CLAG_TYPE_SIZE: {
        size_t v;
        if (!clag__parse_size_str(raw, &v)) return false;
        *(size_t *)f->ref = v;
        break;
    }
 
    case CLAG_TYPE_DOUBLE: {
        double v = strtod(raw, &end);
        if (end == raw || *end != '\0') RET_ERR(CLAG_ERR_INVALID_NUMBER);
        if (errno == ERANGE)            RET_ERR(CLAG_ERR_DOUBLE_OVERFLOW);

        *(double *)f->ref = v;
        return true;
    }
 
    case CLAG_TYPE_FLOAT: {
        float v = strtof(raw, &end);
        if (end == raw || *end != '\0') RET_ERR(CLAG_ERR_INVALID_NUMBER);
        if (errno == ERANGE)            RET_ERR(CLAG_ERR_FLOAT_OVERFLOW);

        *(float *)f->ref = v;
        return true;
    }
 
    case CLAG_TYPE_STR: {
         // raw points into argv; lifetime is fine
        *(char **)f->ref = (char *)raw;
        return true;
    }
 
    case CLAG_TYPE_LIST: {
        ClagList *lst = (ClagList *)f->ref;
        if (!f->list_delim) {
            clag_list_append(lst, (char *)raw);
            return true;
        }

        const char *p = raw;
        for (;;) {
            const char *sep = strchr(p, f->list_delim);
            size_t len = sep ? (size_t)(sep - p) : strlen(p);
            if (len > 0) {
                char *item = (char *)malloc(len + 1);
                assert(item && "clag: out of memory");
                memcpy(item, p, len);
                item[len] = '\0';
                clag_list_append(lst, item);
            }
            if (!sep) break;
            p = sep + 1;
        }
        // } else {
        //     clag_list_append(lst, (char *)raw);
        // }
        break;
    }
 
    default:
        assert(0 && "unreachable");
        return false;
    }
#undef RET_ERR
    return true;
}

static bool clag__apply(Clag *f, const char *raw)
{
    if (f->deprecated)
        fprintf(stderr, "warning: flag -%s is deprecated: %s\n",
                f->name, f->depr_msg ? f->depr_msg : "");
    if (!clag__apply_one(f, raw)) {
        if (!g_clag.error_flag_name) g_clag.error_flag_name = f->name;
        return false;
    }
    // if (!clag__run_validator(f)) return false;
    f->is_set = true;
    return true;
}

// ----------------------------
// Private registration helpers
// ---------------------------- 

static Clag *clag__register(ClagType type, void *external_var, const char *name, char sc, const char *desc)
{
    Clag *f = clag__alloc(type, name, sc, desc);
 
    if (external_var) {
        f->ref = external_var;
        f->ref_is_external = true;
    } else {
        f->ref = clag__val_ptr(&f->val, type);
        f->ref_is_external = false;
    }
    return f;
}

// -----------------------
// Public registration API
// -----------------------
// NOTE: I should turn this into macccccros
bool *clag_bool(const char *name, char sc, bool def, const char *desc)
{
    Clag *f = clag__register(CLAG_TYPE_BOOL, NULL, name, sc, desc);
    f->def.as_bool = def; *(bool *)f->ref = def; return (bool *)f->ref;
}
void clag_bool_var(bool *v, const char *name, char sc, bool def, const char *desc)
{
    Clag *f = clag__register(CLAG_TYPE_BOOL, v, name, sc, desc);
    f->def.as_bool = def; *v = def;
}

float *clag_float(const char *name, char sc, float def, const char *desc)
{
    Clag *f = clag__register(CLAG_TYPE_FLOAT, NULL, name, sc, desc);
    f->def.as_float = def; *(float *)f->ref = def; return (float *)f->ref;
}
void clag_float_var(float *v, const char *name, char sc, float def, const char *desc)
{
    Clag *f = clag__register(CLAG_TYPE_FLOAT, v, name, sc, desc);
    f->def.as_float = def; *v = def;
}

double *clag_double(const char *name, char sc, double def, const char *desc)
{
    Clag *f = clag__register(CLAG_TYPE_DOUBLE, NULL, name, sc, desc);
    f->def.as_double = def; *(double *)f->ref = def; return (double *)f->ref;
}
void clag_double_var(double *v, const char *name, char sc, double def, const char *desc)
{
    Clag *f = clag__register(CLAG_TYPE_DOUBLE, v, name, sc, desc);
    f->def.as_double = def; *v = def;
}

int64_t *clag_int64(const char *name, char sc, int64_t def, const char *desc)
{
    Clag *f = clag__register(CLAG_TYPE_INT64, NULL, name, sc, desc);
    f->def.as_int64 = def; *(int64_t *)f->ref = def; return (int64_t *)f->ref;
}
void clag_int64_var(int64_t *v, const char *name, char sc, int64_t def, const char *desc)
{
    Clag *f = clag__register(CLAG_TYPE_INT64, v, name, sc, desc);
    f->def.as_int64 = def; *v = def;
}

uint64_t *clag_uint64(const char *name, char sc, uint64_t def, const char *desc)
{
    Clag *f = clag__register(CLAG_TYPE_UINT64, NULL, name, sc, desc);
    f->def.as_uint64 = def; *(uint64_t *)f->ref = def; return (uint64_t *)f->ref;
}

void clag_uint64_var(uint64_t *v, const char *name, char sc, uint64_t def, const char *desc)
{
    Clag *f = clag__register(CLAG_TYPE_UINT64, v, name, sc, desc);
    f->def.as_uint64 = def; *v = def;
}

size_t *clag_size(const char *name, char sc, const char *def_str, const char *desc)
{
    Clag *f = clag__register(CLAG_TYPE_SIZE, NULL, name, sc, desc);
    f->def_str = def_str;
    size_t parsed = 0;
    if (def_str) { 
        bool ok = clag__parse_size_str(def_str, &parsed);
        assert(ok && "clag: invalid default size string");
    }
    f->def.as_size = parsed;
    *(size_t *)f->ref = parsed;
    return (size_t *)f->ref;
}

void clag_size_var(size_t *v, const char *name, char sc, const char *def_str, const char *desc)
{
    Clag *f = clag__register(CLAG_TYPE_SIZE, v, name, sc, desc);
    f->def_str = def_str;
    size_t parsed = 0;
    if (def_str) {
        bool ok = clag__parse_size_str(def_str, &parsed);
        assert(ok && "clag: invalid default size string");
    }
    f->def.as_size = parsed; *v = parsed;
}

char **clag_str(const char *name, char sc, const char *def, const char *desc)
{
    Clag *f = clag__register(CLAG_TYPE_STR, NULL, name, sc, desc);
    f->def.as_str = (char *)def; *(char **)f->ref = (char *)def;
    return (char **)f->ref;
}
void clag_str_var(char **v, const char *name, char sc, const char *def, const char *desc)
{
    Clag *f = clag__register(CLAG_TYPE_STR, v, name, sc, desc);
    f->def.as_str = (char *)def; *v = (char *)def;
}
 
ClagList *clag_list(const char *name, char sc, char delim, const char *desc)
{
    Clag *f = clag__register(CLAG_TYPE_LIST, NULL, name, sc, desc);
    f->list_delim = delim;
    memset(f->ref, 0, sizeof(ClagList));
    return (ClagList *)f->ref;
}
void clag_list_var(ClagList *v, const char *name, char sc, char delim, const char *desc)
{
    Clag *f = clag__register(CLAG_TYPE_LIST, v, name, sc, desc);
    f->list_delim = delim;
    memset(v, 0, sizeof(ClagList));
}

// --------------
// Flag modifiers
// --------------

#define CLAG__LOOKUP(name) \
    Clag *f = clag__find_long(name); \
    assert(f && "clag modifier: unknown flag name")

void clag_required(const char *name)
{
    CLAG__LOOKUP(name);
    f->required = true;
}
void clag_hidden(const char *name)
{
    CLAG__LOOKUP(name);
    f->hidden = true;
}
void clag_deprecated(const char *name, const char *msg)
{
    CLAG__LOOKUP(name);
    f->deprecated = true;
    f->depr_msg = msg;
}
void clag_usage(const char *synopsis) { g_clag.usage_synopsis = synopsis; }

#undef CLAG__LOOKUP

// ------
// Parser
// ------

bool clag_parse(int argc, char **argv)
{
    g_clag.program_name    = argc > 0 ? argv[0] : "";
    g_clag.error           = CLAG_OK;
    g_clag.error_flag_name = NULL;
    g_clag.error_detail    = NULL;

    g_clag.rest_argc = 0;
    g_clag.rest_argv = malloc(sizeof(char*) * (size_t)argc);
    assert(g_clag.rest_argv && "clag: out of memory");

    for (int i = 1; i < argc; i++) {
        char *arg = argv[i];
 
        if (strcmp(arg, "--") == 0) {
            for (int j = i + 1; j < argc; j++)
                g_clag.rest_argv[g_clag.rest_argc++] = argv[j];
            break;
        }

        // not a flag → positional (BUT keep parsing later flags)
        if (arg[0] != '-' || arg[1] == '\0') {
            g_clag.rest_argv[g_clag.rest_argc++] = arg;
            continue;
        }
 
        // Strip one or two leading dashes
        char *name_start = arg + 1;
        bool is_long = (*name_start == '-');
        if (is_long) name_start++;
 
        // Split on '=' without mutating argv (argv may contain string literals)
        const char *eq = strchr(name_start, '=');
        size_t name_len = eq ? (size_t)(eq - name_start) : strlen(name_start);
         // Priority:
        //   1. name_len==1 and char is a registered short -> short-flag path
        //   2. name_len>1  and first char is a non-bool short -> -oFILE value
        //   3. name_len>1  and all chars are bool shorts -> -abc cluster
        //   4. Everything else -> long-name lookup (covers -n where n is long
        //      name with no short char, and -verbose etc.)
        if (!is_long && !eq) {
            Clag *sf0 = clag__find_short(name_start[0]);

            if (name_len == 1 && sf0) {
                // Case 1: single short char
                if (sf0->type == CLAG_TYPE_BOOL) {
                    if (!clag__apply(sf0, "true")) return false;
                } else {
                    if (i + 1 >= argc) {
                        g_clag.error = CLAG_ERR_NO_VALUE;
                        g_clag.error_flag_name = sf0->name;
                        return false;
                    }
                    if (!clag__apply(sf0, argv[++i])) return false;
                }
                continue;
            }

            if (name_len > 1 && sf0 && sf0->type != CLAG_TYPE_BOOL) {
                // Case 2: -oFILE - only if the full token is NOT a long flag name.
                // E.g. "-out" with short 'o' and long flag "out": prefer long.
                if (!clag__find_long(name_start)) {
                    if (!clag__apply(sf0, name_start + 1)) return false;
                    continue;
                }
            }

            if (name_len > 1 && sf0 && sf0->type == CLAG_TYPE_BOOL) {
                // Case 3: try bool cluster -abc
                bool all_bool = true;
                for (size_t k = 0; k < name_len; k++) {
                    Clag *bf = clag__find_short(name_start[k]);
                    if (!bf || bf->type != CLAG_TYPE_BOOL) { all_bool = false; break; }
                }
                if (all_bool) {
                    for (size_t k = 0; k < name_len; k++) {
                        Clag *bf = clag__find_short(name_start[k]);
                        if (!clag__apply(bf, "true")) return false;
                    }
                    continue;
                }
            }
        }

        // Copy the flag name into a local buffer so we never write to argv.
        char name_buf[128];
        if (name_len >= sizeof(name_buf)) {
            g_clag.error = CLAG_ERR_UNKNOWN_FLAG;
            g_clag.error_flag_name = name_start;
            return false;
        }
        memcpy(name_buf, name_start, name_len);
        name_buf[name_len] = '\0';

        // Auto --help / -h
        if (strcmp(name_buf, "help") == 0 ||
            (!is_long && name_buf[0] == 'h' && name_buf[1] == '\0')) {
            clag_print_help(stdout);
            exit(0);
        }

        Clag *f = clag__find_long(name_buf);
        if (!f && !is_long && name_len == 1)
            f = clag__find_short(name_buf[0]);
        if (!f) {
            g_clag.error = CLAG_ERR_UNKNOWN_FLAG;
            g_clag.error_flag_name = name_start;
            return false;
        }

        const char *inline_val = eq ? eq + 1 : NULL;
        if (f->type == CLAG_TYPE_BOOL && !inline_val) {
            if (!clag__apply(f, "true")) return false;
            continue;
        }

        // Grab value
        const char *val = inline_val;
        if (!val) {
            if (i + 1 >= argc) {
                g_clag.error = CLAG_ERR_NO_VALUE;
                g_clag.error_flag_name = f->name;
                return false;
            }
            val = argv[++i];
        }
        if (!clag__apply(f, val)) return false;
    }

    // Check required flags
    for (size_t fi = 0; fi < g_clag.flags_count; fi++) {
        Clag *f = &g_clag.flags[fi];
        if (f->required && !f->is_set) {
            g_clag.error = CLAG_ERR_REQUIRED;
            g_clag.error_flag_name = f->name;
            return false;
        }
    }
 
    return true;
}

// ---------
// Accessors
// ---------
 
int    clag_rest_argc(void)    { return g_clag.rest_argc; }
char **clag_rest_argv(void)    { return g_clag.rest_argv; }
const char *clag_program_name(void) { return g_clag.program_name; }
 
const char *clag_name(void *val)
{
    for (size_t i = 0; i < g_clag.flags_count; i++) {
        if (g_clag.flags[i].ref == val)
            return g_clag.flags[i].name;
    }
    return NULL;
}

bool clag_is_set(const char *name)
{
    Clag *f = clag__find_long(name);
    return f && f->is_set;
}

// -----------
// Diagnostics
// -----------

void clag_print_error(FILE *s)
{
    if (g_clag.error == CLAG_OK) return;
 
    const char *prog = g_clag.program_name    ? g_clag.program_name    : "";
    const char *flag = g_clag.error_flag_name ? g_clag.error_flag_name : "(unknown)";
 
    switch (g_clag.error) {
    case CLAG_ERR_UNKNOWN_FLAG:
        fprintf(s, "%s: unknown flag: -%s\n", prog, flag);
        break;
    case CLAG_ERR_NO_VALUE:
        fprintf(s, "%s: flag needs an argument: -%s\n", prog, flag);
        break;
    case CLAG_ERR_INVALID_NUMBER:
        fprintf(s, "%s: invalid number for flag -%s\n", prog, flag);
        break;
    case CLAG_ERR_INT_OVERFLOW:
        fprintf(s, "%s: integer overflow for flag -%s\n", prog, flag);
        break;
    case CLAG_ERR_INT_UNDERFLOW:
        fprintf(s, "%s: integer underflow for flag -%s\n", prog, flag);
        break;
    case CLAG_ERR_FLOAT_OVERFLOW:
        fprintf(s, "%s: float overflow for flag -%s\n", prog, flag);
        break;
    case CLAG_ERR_DOUBLE_OVERFLOW:
        fprintf(s, "%s: double overflow for flag -%s\n", prog, flag);
        break;
    case CLAG_ERR_INVALID_SIZE_SUFFIX:
        fprintf(s, "%s: invalid size suffix for flag -%s (valid: K/M/G/T/P)\n", prog, flag);
        break;
    case CLAG_ERR_INVALID_BOOL:
        fprintf(s, "%s: invalid boolean for flag -%s (valid: true/false/1/0/yes/no/on/off)\n", prog, flag);
        break;
    case CLAG_ERR_REQUIRED:
        fprintf(s, "%s: required flag not provided: -%s\n", prog, flag);
        break;
    default:
        fprintf(s, "%s: unknown error for flag -%s\n", prog, flag);
        break;
    }
}

// Print the default value of a flag in a human-readable form.
static void clag__print_default(FILE *s, const Clag *f)
{
    switch (f->type) {
    case CLAG_TYPE_BOOL:
        fprintf(s, f->def.as_bool ? "true" : "false");
        break;
    case CLAG_TYPE_INT64:
        fprintf(s, "%" PRId64, f->def.as_int64);
        break;
    case CLAG_TYPE_UINT64:
        fprintf(s, "%" PRIu64, f->def.as_uint64);
        break;
    case CLAG_TYPE_DOUBLE:
        fprintf(s, "%g", f->def.as_double);
        break;
    case CLAG_TYPE_FLOAT:
        fprintf(s, "%g", (double)f->def.as_float);
        break;
    case CLAG_TYPE_SIZE:
        if (f->def_str)
            fprintf(s, "%s", f->def_str);
        else
            fprintf(s, "%zu", f->def.as_size);
        break;
    case CLAG_TYPE_STR:
        if (f->def.as_str)
            fprintf(s, "\"%s\"", f->def.as_str);
        else
            fprintf(s, "\"\"");
        break;
    case CLAG_TYPE_LIST:
        fprintf(s, "[]");
        break;
    default:
        break;
    }
}
 
static const char *clag__type_name(ClagType t)
{
    switch (t) {
    case CLAG_TYPE_BOOL:   return "bool";
    case CLAG_TYPE_INT64:  return "int64";
    case CLAG_TYPE_UINT64: return "uint64";
    case CLAG_TYPE_DOUBLE: return "float64";
    case CLAG_TYPE_FLOAT:  return "float32";
    case CLAG_TYPE_SIZE:   return "size";
    case CLAG_TYPE_STR:    return "string";
    case CLAG_TYPE_LIST:   return "list";
    default:               return "?";
    }
}

// Word-wrap `text` at `width` columns, indenting continuation lines by `indent`.
static void clag__wrap(FILE *s, const char *text, int indent, int width)
{
    if (!text || !*text) { fputc('\n', s); return; }
    // We're already at column `indent` (caller printed the prefix).
    int col = indent;
    const char *p = text;
    while (*p) {
        if (*p == '\n') { fputc('\n', s); for (int j=0;j<indent;j++) fputc(' ',s); col=indent; p++; continue; }
        const char *word = p;
        while (*p && *p != ' ' && *p != '\n') p++;
        int wlen = (int)(p - word);
        if (col + (col > indent ? 1 : 0) + wlen > width && col > indent) {
            fputc('\n', s);
            for (int j = 0; j < indent; j++) fputc(' ', s);
            col = indent;
        } else if (col > indent) { fputc(' ', s); col++; }
        fwrite(word, 1, (size_t)wlen, s);
        col += wlen;
        if (*p == ' ') p++;
    }
    fputc('\n', s);
}

void clag_print_options(FILE *s)
{
    // Compute column widths for aligned output.
    size_t max_name = 4; // minimum "flag" header width
    for (size_t i = 0; i < g_clag.flags_count; i++) {
        if (g_clag.flags[i].hidden) continue;
        size_t n = strlen(g_clag.flags[i].name);
        if (n > max_name) max_name = n;
    }
    int nw = (int)max_name;

    // Each row: "  -s, --longname  type    desc"
    //            ^4  ^2  ^2+nw    ^2+8+2
    int prefix_w = 4 + 2 + 2 + nw + 2 + 8 + 2;
 

    for (size_t i = 0; i < g_clag.flags_count; i++) {
        const Clag *f = &g_clag.flags[i];
        if (f->hidden) continue;

        // Flag+short column
        if (f->short_char)
            fprintf(s, "  -%c, --%-*s", f->short_char, nw, f->name);
        else
            fprintf(s, "      --%-*s", nw, f->name);

        // Type column (fixed 8 chars)
        fprintf(s, "  %-8s  ", clag__type_name(f->type));

        // Build description with metadata appended
        // We assemble into a buffer then word-wrap.
        char desc_buf[1024];
        int pos = 0;
        int rem = (int)sizeof desc_buf;

#define DAPP(...)                                                 \
    do {                                                          \
        int n = snprintf(desc_buf+pos, (size_t)rem, __VA_ARGS__); \
        if (n > 0) {                                              \
            pos += n;                                             \
            rem -= n;                                             \
        }} while(0)

        if (f->desc) DAPP("%s", f->desc);

        // Default (not for list)
        if (f->type != CLAG_TYPE_LIST) {
            DAPP(" [default: ");
            // Capture clag__print_default into buffer
            FILE *tmp = fmemopen(desc_buf + pos, (size_t)rem, "w");
            if (tmp) {
                clag__print_default(tmp, f);
                fclose(tmp);
                int written = (int)strlen(desc_buf + pos);
                pos += written; rem -= written;
            }
            DAPP("]");
        }
        if (f->required)   DAPP(" (required)");
        if (f->deprecated) DAPP(" [DEPRECATED]");

#undef DAPP

        desc_buf[sizeof desc_buf - 1] = '\0';
        clag__wrap(s, desc_buf, prefix_w, CLAG_HELP_WIDTH);
    }
}

void clag_print_help(FILE *s)
{
    const char *prog = g_clag.program_name    ? g_clag.program_name    : "program";
    const char *syn  = g_clag.usage_synopsis  ? g_clag.usage_synopsis  : "[options]";
    fprintf(s, "Usage: %s %s\n\nOptions:\n", prog, syn);
    clag_print_options(s);
}

#endif // CLAG_IMPLEMENTATION

/*
# Changelog

      2.0.0 (2026-03-29)
                     - BREAKING: New API with short flag support (`char sc`)
                     - BREAKING: clag_size now takes default as string (e.g. "4M")
                     - BREAKING: clag_list supports delimiter-based splitting
                     - Add short flag parsing (-v, -abc, -oFILE)
                     - Add boolean clustering (-abc for multiple bool flags)
                     - Add inline short value support (-oFILE)
                     - Add flag modifiers:
                         - clag_required()
                         - clag_hidden()
                         - clag_deprecated()
                         - clag_usage()
                     - Add clag_is_set() to detect explicitly provided flags
                     - Add automatic --help / -h handling
                     - Add clag_print_help() with usage + formatted options
                     - Add word-wrapped help output (CLAG_HELP_WIDTH)
                     - Improve help formatting (aligned columns, metadata display)
                     - Add default string preservation for size types
                     - Add delimiter-based list parsing (e.g. -tag=a,b,c)
                     - Add deprecation warnings at parse time
                     - Add required flag validation
                     - Add integer underflow detection
                     - Improve error reporting with more precise diagnostics
                     - Improve parsing logic (short vs long disambiguation)
                     - Remove unsafe argv mutation (fully const-safe parsing)
                     - Improve memory safety in list handling (checked realloc)
                     - Add clag_list_free() helper
                     - Add duplicate flag / short detection (assert)
                     - Track flag "is_set" state internally
                     - Internal refactor:
                         - split clag__apply -> clag__apply_one + wrapper
                         - separate long/short lookup paths
                         - cleaner registration pipeline
                     - Improve test compatibility with new API

      1.1.0 (2026-03-25)
                     - Make clag_name() O(1) using pointer lookup table
                     - Add new type: int64 (clag_int64 / _var)
                     - Add internal pointer->name mapping for fast reverse lookup

      1.0.0 (2026-03-24)
                     - Initial release
                     - Core flag system
                     - All supported types
                     - Inline and separate parsing
                     - Boolean shorthand
                     - Size suffix parsing
                     - External variable binding
                     - Rest arguments support
                     - Error reporting and help output
*/

/*
# Version Conventions

  We follow https://semver.org/:

  MAJOR.MINOR.PATCH

  - PATCH: bug fixes, no API changes
  - MINOR: backward-compatible additions
  - MAJOR: breaking changes / cleanup

  Breaking changes in MINOR are considered bugs.
*/

/*
   ------------------------------------------------------------------------------
   This software is available under 2 licenses -- choose whichever you prefer.
   ------------------------------------------------------------------------------
   ALTERNATIVE A - MIT License
   Copyright (c) 2026 Rama Maulana
   Permission is hereby granted, free of charge, to any person obtaining a copy of
   this software and associated documentation files (the "Software"), to deal in
   the Software without restriction, including without limitation the rights to
   use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
   of the Software, and to permit persons to whom the Software is furnished to do
   so, subject to the following conditions:
   The above copyright notice and this permission notice shall be included in all
   copies or substantial portions of the Software.
   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
   IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
   FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
   AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
   LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
   OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
   SOFTWARE.
   ------------------------------------------------------------------------------
   ALTERNATIVE B - Public Domain (www.unlicense.org)
   This is free and unencumbered software released into the public domain.
   Anyone is free to copy, modify, publish, use, compile, sell, or distribute this
   software, either in source code form or as a compiled binary, for any purpose,
   commercial or non-commercial, and by any means.
   In jurisdictions that recognize copyright laws, the author or authors of this
   software dedicate any and all copyright interest in the software to the public
   domain. We make this dedication for the benefit of the public at large and to
   the detriment of our heirs and successors. We intend this dedication to be an
   overt act of relinquishment in perpetuity of all present and future rights to
   this software under copyright law.
   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
   IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
   FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
   AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
   ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
   WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
   ------------------------------------------------------------------------------
*/
