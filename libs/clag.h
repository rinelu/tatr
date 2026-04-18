/*
   clag - v3.0.0 - Public Domain - Single-header CLI parser

   A tiny argument parsing library for C.

   # Quick Example

      ```c
      #define CLAG_IMPLEMENTATION
      #include "clag.h"

      int main(int argc, char **argv)
      {
          bool *verbose = clag_bool("verbose", 'v', false, "enable logging");
          uint64_t *count = clag_uint64("count", 'n', 10, "iteration count");

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
      - Non-flag arguments are collected as rest args (parsing continues)
      - Boolean flags can be negated with "--no-<name>"
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
            required flag missing
            mutually exclusive flags
            missing dependency
            invalid enum choice
            custom validator failed
            value out of range

   # Configuration

      Optional macros:

            CLAG_CAP
                  Maximum number of flags (default: 256)

            CLAG_DA_INIT_CAP
                  Initial list capacity (default: 8)

            CLAG_HELP_WIDTH
                  Word-wrap width for help output (default: 80)

            CLAG_MUTEX_CAP
                  Maximum number of mutex groups (default: 32)

            CLAG_MUTEX_MEMBER_CAP
                  Maximum flags per mutex group (default: 16)

            CLAG_EXAMPLE_CAP
                  Maximum number of clag_example() entries (default: 16)

            CLAG_GROUP_CAP
                  Maximum number of option groups (default: 32)

            CLAG_ALIAS_CAP
                  Maximum number of flag aliases (default: 64)

            CLAG_VALIDATOR_ERRBUF_SIZE
                  Size of the error message buffer passed to custom validators
                  (default: 256)
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
#include <stdarg.h>

#ifndef CLAG_CAP
#define CLAG_CAP 256
#endif

#ifndef CLAG_DA_INIT_CAP
#define CLAG_DA_INIT_CAP 256
#endif

#ifndef CLAG_HELP_WIDTH
#define CLAG_HELP_WIDTH 80
#endif

#ifndef CLAG_MUTEX_CAP
#define CLAG_MUTEX_CAP 32
#endif

#ifndef CLAG_MUTEX_MEMBER_CAP
#define CLAG_MUTEX_MEMBER_CAP 16
#endif

#ifndef CLAG_EXAMPLE_CAP
#define CLAG_EXAMPLE_CAP 16
#endif

#ifndef CLAG_GROUP_CAP
#define CLAG_GROUP_CAP 32
#endif

#ifndef CLAG_ALIAS_CAP
#define CLAG_ALIAS_CAP 64
#endif

#ifndef CLAG_VALIDATOR_ERRBUF_SIZE
#define CLAG_VALIDATOR_ERRBUF_SIZE 256
#endif

#ifdef __cplusplus
#define CLAG_DECLTYPE(T) (decltype(T))
#else
#define CLAG_DECLTYPE(T)
#endif // __cplusplus

#define clag_da_reserve(da, item)                                           \
    do {                                                                    \
        if ((da)->count >= (da)->capacity) {                                \
            size_t _nc = (da)->capacity == 0                                \
                             ? CLAG_DA_INIT_CAP : (da)->capacity * 2;       \
            (da)->items = CLAG_DECLTYPE((da)->items)realloc((da)->items, _nc * sizeof(*(da)->items));  \
            assert((da)->items != NULL && "clag: out of memory");           \
            (da)->capacity = _nc;                                           \
        }                                                                   \
    } while (0)

#define clag_da_append(da, item)                 \
    do {                                         \
        clag_da_reserve((da), (da)->count + 1);  \
        (da)->items[(da)->count++] = (item);     \
    } while (0)

#define clag_da_free(da)                         \
    do { free((da)->items);                      \
         (da)->items = NULL;                     \
         (da)->count = (da)->capacity = 0; } while(0)

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
} Clag_Type;

static_assert(COUNT_CLAG_TYPES == 8, "clag__apply/clag__type_name/clag__print_default needs updating");

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
    CLAG_ERR_MUTEX,       // mutually exclusive flags both set
    CLAG_ERR_DEPENDS,     // dependency flag not set
    CLAG_ERR_ENUM,        // value not in allowed choices
    CLAG_ERR_RANGE,       // value outside [lo, hi]
    CLAG_ERR_CUSTOM,      // custom validator returned false
    COUNT_CLAG_ERRORS,
} Clag_Error;

typedef struct {
    const char **items;
    size_t count;
    size_t capacity;
} Clag_List;

typedef union {
    char     *as_str;
    int64_t   as_int64;
    uint64_t  as_uint64;
    double    as_double;
    float     as_float;
    bool      as_bool;
    size_t    as_size;
    Clag_List  as_list;
} Clag_Value;

// Range constraint (stored as doubles; cast back on use)
typedef struct {
    bool   active;
    double lo;
    double hi;
} Clag_Range;

typedef struct {
    char *items;
    size_t count;
    size_t capacity;
} Clag_ErrBuf;

typedef bool (*ClagValidatorFn)(const char *name, void *val, char *errbuf, size_t errbuf_sz);

typedef struct {
    Clag_Type    type;
    const char *name;
    char        short_char;
    const char *desc;

    Clag_Value  val;
    void      *ref;
    bool       ref_is_external;

    Clag_Value  def; 
    const char *def_str; // raw string for size default display

    char list_delim;

    // modifiers
    bool        required;
    bool        hidden;
    bool        deprecated;
    const char *depr_msg;

    // built-in constraints (checked by clag__run_validation)
    Clag_Range    range;
    const char **choices; // NULL-terminated; only for STR

    // custom validator (checked last in clag__run_validation)
    ClagValidatorFn validator;
    char            validator_errbuf[CLAG_VALIDATOR_ERRBUF_SIZE];
    
    bool is_set;
    bool seen;

    // option group index (-1 = ungrouped)
    int group_idx;
} Clag;

// Mutex group
typedef struct {
    const char *members[CLAG_MUTEX_MEMBER_CAP];
    size_t      count;
} Clag_MutexGroup;

// Dependency pair
typedef struct {
    const char *name;
    const char *requires;
} Clag_Depend;

// Option group label
typedef struct {
    const char *label;
    // first flag index that belongs to this group (for ordering in print)
    size_t first_flag_idx;
} Clag_GroupDef;

// Alias entry
typedef struct {
    const char *alias;
    const char *primary;
} Clag_Alias;

typedef struct {
    Clag   flags[CLAG_CAP];
    size_t flags_count;

    Clag_Error   error;
    const char *error_flag_name;
    const char *error_detail;

    const char *program_name;
    const char *usage_synopsis;
    const char *version_string;  // set by clag_version()

    int    rest_argc;
    char **rest_argv;

    // mutex groups
    Clag_MutexGroup mutex_groups[CLAG_MUTEX_CAP];
    size_t         mutex_groups_count;

    // dependency pairs
    Clag_Depend depends[CLAG_CAP];
    size_t     depends_count;

    // examples
    const char *examples[CLAG_EXAMPLE_CAP];
    size_t      examples_count;

    // option groups
    Clag_GroupDef groups[CLAG_GROUP_CAP];
    size_t       groups_count;
    int          current_group; // -1 = none

    Clag_Alias aliases[CLAG_ALIAS_CAP];
    size_t    aliases_count;
} Clag_Context;

#ifdef CLAG_IMPLEMENTATION
static Clag_Context clag_global_context = { .current_group = -1 };
#else
extern Clag_Context clag_global_context;
#endif // CLAG_IMPLEMENTATION

// Register a flag and return a pointer to its storage.
// sc (short char): pass 0 for no short form.
// def (default): pass NULL for no short form.
bool     *clag_bool  (const char *name, char sc, bool        def, const char *desc);
int64_t  *clag_int64 (const char *name, char sc, int64_t     def, const char *desc);
uint64_t *clag_uint64(const char *name, char sc, uint64_t    def, const char *desc);
float    *clag_float (const char *name, char sc, float       def, const char *desc);
double   *clag_double(const char *name, char sc, double      def, const char *desc);
size_t   *clag_size  (const char *name, char sc, const char *def, const char *desc);
char    **clag_str   (const char *name, char sc, const char *def, const char *desc);
// delim: character splitting repeated values (0 = no splitting, ',' is typical).
Clag_List *clag_list  (const char *name, char sc, char        delim, const char *desc);

// Register a flag and store results into a caller-provided variable.
void clag_bool_var  (bool     *v, const char *name, char sc, bool        def, const char *desc);
void clag_int64_var (int64_t  *v, const char *name, char sc, int64_t     def, const char *desc);
void clag_uint64_var(uint64_t *v, const char *name, char sc, uint64_t    def, const char *desc);
void clag_float_var (float    *v, const char *name, char sc, float       def, const char *desc);
void clag_double_var(double   *v, const char *name, char sc, double      def, const char *desc);
void clag_size_var  (size_t   *v, const char *name, char sc, const char *def, const char *desc);
void clag_str_var   (char    **v, const char *name, char sc, const char *def, const char *desc);
void clag_list_var  (Clag_List *v, const char *name, char sc, char        delim, const char *desc);

// Flag modifiers
// call after registration, before clag_parse
void clag_required  (const char *name);                  // fail if not provided
void clag_deprecated(const char *name, const char *msg); // warn on use
void clag_hidden    (const char *name);                  // hide from --help

// Value constraints
// Checked inside clag__run_validation() after parsing.

// For int64 / uint64 / double flags
// fail if the parsed value is outside [lo, hi].
void clag_range_int64 (const char *name, int64_t  lo, int64_t  hi);
void clag_range_uint64(const char *name, uint64_t lo, uint64_t hi);
void clag_range_double(const char *name, double   lo, double   hi);

// Enum / choice validation for string flags.
// `choices` must be a NULL-terminated array.
//
// Use clag_choices(...) for convenience:
//   clag_choices("mode", "fast", "slow");
void  clagc__choices(Clag_Context *cx, const char *name, const char **choices);
void  clag__choices(const char *name, const char **choices);
#define CLAG__DEFINE_CHOICES(...)                      \
    static const char *_clag_choices_##__LINE__[] = {  \
        __VA_ARGS__, NULL                              \
    };
#define  clag_choices(name, ...)      \
    CLAG__DEFINE_CHOICES(__VA_ARGS__) \
    clag__choices(name, _clag_choices_##__LINE__);
#define  clagc_choices(cx, name, ...)     \
    CLAG__DEFINE_CHOICES(__VA_ARGS__)     \
    clagc__choices(cx, name, _clag_choices_##__LINE__);

// Custom validator hook.
// Called after type parsing and built-in constraint checks succeed.
// Signature: bool fn(name, value_ptr, errbuf, errbuf_sz)
//   value_ptr : points to the parsed value (e.g. int64_t*, char**, ...)
//   errbuf    : fill with a human-readable message on failure; may be NULL
//   return true on success, false to fail with CLAG_ERR_CUSTOM
// Only one validator per flag.
void clag_validator(const char *name, ClagValidatorFn fn);

// Flag alias: --alias is accepted as an alternative long name for --name.
// Resolves transparently for parsing, clag_is_set(), and clag_was_seen().
void clag_alias(const char *name, const char *alias);

// Constraint groups

// Mutually exclusive: at most one of the named flags may be set.
// Pass a NULL-terminated list of flag names.
void clag_mutex(const char *first, ...);

// Dependency: if `name` is set, `requires` must also be set.
void clag_depends(const char *name, const char *requires);

// Help / display helpers
void clag_usage  (const char *synopsis); // synopsis for help header
void clag_example(const char *text);     // add an example line to --help

// Begin a named option group. Flags registered after this call until the next
// clag_group() or end of registration are listed under that group header.
// Pass NULL to reset to the ungrouped section.
void clag_group(const char *label);

// Register an automatic --version / -V flag.
// When seen, prints "<program> <version>\n" and exits 0.
void clag_version(const char *version);

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

// Output
void clag_print_error  (FILE *stream);
void clag_print_help   (FILE *stream);
void clag_print_options(FILE *stream);

// Lookup the registered name for an arbitrary value pointer
// Useful for custom error messages. Return NULL if not found.
const char *clag_name(void *val);

// True if the named flag (or alias) was explicitly supplied on the command line.
bool clag_is_set(const char *name);

// True if the token was seen (even if value parsing failed).
bool clag_was_seen(const char *name);

// Reset all parser state (flags, groups, constraints, examples).
// Frees internal allocations, including list storage and their elements.
// Safe to call between parses (useful in tests).
void clag_reset(void);

// Public iteration over non-hidden flags.
size_t clag_count(void);
// Returns NULL if out of range.
const char *clag_flag_name_at(size_t i);
const char *clag_flag_desc_at(size_t i);

// Context-specific versions of the API.
//
// These functions operate on a user-provided Clag_Context instead of the global one,
// allowing multiple independent parsers in the same program.
//
// NOTE: clagc_choices macros are defined above.
bool     *clagc_bool  (Clag_Context *cx, const char *name, char sc, bool        def, const char *desc);
int64_t  *clagc_int64 (Clag_Context *cx, const char *name, char sc, int64_t     def, const char *desc);
uint64_t *clagc_uint64(Clag_Context *cx, const char *name, char sc, uint64_t    def, const char *desc);
float    *clagc_float (Clag_Context *cx, const char *name, char sc, float       def, const char *desc);
double   *clagc_double(Clag_Context *cx, const char *name, char sc, double      def, const char *desc);
size_t   *clagc_size  (Clag_Context *cx, const char *name, char sc, const char *def, const char *desc);
char    **clagc_str   (Clag_Context *cx, const char *name, char sc, const char *def, const char *desc);
Clag_List *clagc_list  (Clag_Context *cx, const char *name, char sc, char        delim, const char *desc);

void clagc_bool_var  (Clag_Context *cx, bool     *v, const char *name, char sc, bool        def, const char *desc);
void clagc_int64_var (Clag_Context *cx, int64_t  *v, const char *name, char sc, int64_t     def, const char *desc);
void clagc_uint64_var(Clag_Context *cx, uint64_t *v, const char *name, char sc, uint64_t    def, const char *desc);
void clagc_float_var (Clag_Context *cx, float    *v, const char *name, char sc, float       def, const char *desc);
void clagc_double_var(Clag_Context *cx, double   *v, const char *name, char sc, double      def, const char *desc);
void clagc_size_var  (Clag_Context *cx, size_t   *v, const char *name, char sc, const char *def, const char *desc);
void clagc_str_var   (Clag_Context *cx, char    **v, const char *name, char sc, const char *def, const char *desc);
void clagc_list_var  (Clag_Context *cx, Clag_List *v, const char *name, char sc, char        delim, const char *desc);

void clagc_required  (Clag_Context *cx, const char *name);
void clagc_deprecated(Clag_Context *cx, const char *name, const char *msg);
void clagc_hidden    (Clag_Context *cx, const char *name);

void clagc_range_int64 (Clag_Context *cx, const char *name, int64_t  lo, int64_t  hi);
void clagc_range_uint64(Clag_Context *cx, const char *name, uint64_t lo, uint64_t hi);
void clagc_range_double(Clag_Context *cx, const char *name, double   lo, double   hi);

void clagc_validator(Clag_Context *cx, const char *name, ClagValidatorFn fn);

void clagc_alias(Clag_Context *cx, const char *name, const char *alias);
void clagc_mutex(Clag_Context *cx, const char *first, ...);
void clagc_depends(Clag_Context *cx, const char *name, const char *requires);

void clagc_usage  (Clag_Context *cx, const char *synopsis);
void clagc_example(Clag_Context *cx, const char *text);
void clagc_group(Clag_Context *cx, const char *label);

void clagc_version(Clag_Context *cx, const char *version);

bool clagc_parse(Clag_Context *cx, int argc, char **argv);

int    clagc_rest_argc(Clag_Context *cx);
char **clagc_rest_argv(Clag_Context *cx);
const char *clagc_program_name(Clag_Context *cx);

void clagc_print_error  (Clag_Context *cx, FILE *stream);
void clagc_print_help   (Clag_Context *cx, FILE *stream);
void clagc_print_options(Clag_Context *cx, FILE *stream);

const char *clagc_name(Clag_Context *cx, void *val);

bool clagc_is_set(Clag_Context *cx, const char *name);
bool clagc_was_seen(Clag_Context *cx, const char *name);
void clagc_reset(Clag_Context *cx);

size_t clagc_count(Clag_Context *cx);
const char *clagc_flag_name_at(Clag_Context *cx, size_t i);
const char *clagc_flag_desc_at(Clag_Context *cx, size_t i);

#endif // CLAG_H_

#ifdef CLAG_IMPLEMENTATION

// ---------------------------------------------------------------------------
// Write-callback helpers
// ---------------------------------------------------------------------------

typedef void (*Clag__WriteFn)(void *ctx, const char *str);

typedef struct {
    char *buf;
    int  *pos;
    int  *rem;
} Clag__BufCtx;

static void clag__buf_write(void *ctx, const char *str)
{
    Clag__BufCtx *b = (Clag__BufCtx*)ctx;
    int n = snprintf(b->buf + *b->pos, (size_t)*b->rem, "%s", str);
    if (n > 0 && n < *b->rem) {
        *b->pos += n;
        *b->rem -= n;
    }
}

static void clag__writef(Clag__WriteFn fn, void *ctx, const char *fmt, ...)
{
    char buf[128];
    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    if (n <= 0) return;
    if ((size_t)n < sizeof(buf)) {
        fn(ctx, buf);
        return;
    }
    char *tmp = (char *)malloc((size_t)n + 1);
    if (!tmp) return;
    va_start(ap, fmt);
    vsnprintf(tmp, (size_t)n + 1, fmt, ap);
    va_end(ap);
    fn(ctx, tmp);
    free(tmp);
}

// ---------------------------------------------------------------------------
// Internal lookups
// ---------------------------------------------------------------------------

static Clag *clag__find_long(Clag_Context *cx, const char *name)
{
    for (size_t i = 0; i < cx->flags_count; i++)
        if (strcmp(cx->flags[i].name, name) == 0)
            return &cx->flags[i];
    return NULL;
}

static Clag *clag__find_short(Clag_Context *cx, char sc)
{
    for (size_t i = 0; i < cx->flags_count; i++)
        if (cx->flags[i].short_char == sc)
            return &cx->flags[i];
    return NULL;
}

// Resolve alias to primary, then fall back to direct long lookup.
static Clag *clag__find_by_name(Clag_Context *cx, const char *name)
{
    for (size_t i = 0; i < cx->aliases_count; i++)
        if (strcmp(cx->aliases[i].alias, name) == 0)
            return clag__find_long(cx, cx->aliases[i].primary);
    return clag__find_long(cx, name);
}

static void *clag__val_ptr(Clag_Value *v, Clag_Type t)
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

// ---------------------------------------------------------------------------
// Size / bool parsers
// ---------------------------------------------------------------------------

static bool clag__size_suffix(Clag_Context *cx, const char *endptr, unsigned long long *out_mult)
{
    *out_mult = 1;
    if (*endptr == '\0') return true;
    char letter = *endptr;
    if (letter >= 'a' && letter <= 'z') letter = (char)(letter - 32);
    unsigned long long m;
    switch (letter) {
        case 'K': m = 1ULL << 10; break;
        case 'M': m = 1ULL << 20; break;
        case 'G': m = 1ULL << 30; break;
        case 'T': m = 1ULL << 40; break;
        case 'P': m = 1ULL << 50; break;
        default:  cx->error = CLAG_ERR_INVALID_SIZE_SUFFIX; return false;
    }
    endptr++;
    if (*endptr == 'i') endptr++;
    if (*endptr == 'B' || *endptr == 'b') endptr++;
    if (*endptr != '\0') { cx->error = CLAG_ERR_INVALID_SIZE_SUFFIX; return false; }
    *out_mult = m;
    return true;
}

static bool clag__parse_size_str(Clag_Context *cx, const char *raw, size_t *out)
{
#define RET_ERR(kind) { cx->error = kind; return false; }
    errno = 0;
    char *end;
    unsigned long long v = strtoull(raw, &end, 0);
    if (end == raw)      RET_ERR(CLAG_ERR_INVALID_NUMBER);
    if (errno == ERANGE) RET_ERR(CLAG_ERR_INT_OVERFLOW);
    unsigned long long m;
    if (!clag__size_suffix(cx, end, &m)) return false;
    if (m > 1 && v > (unsigned long long)SIZE_MAX / m)
        RET_ERR(CLAG_ERR_INT_OVERFLOW);
    *out = (size_t)(v * m);
    return true;
#undef RET_ERR
}

// Parse a boolean string.
// Accepts: 1/0, true/false, yes/no, on/off (case-insensitive).
static bool clag__parse_bool(Clag_Context *cx, const char *s, bool *out)
{
#define RET_TRUE  { *out=true; return true; }
#define RET_FALSE { *out=false; return true; }
    if (s[1] == '\0') {
        if (*s == '1') RET_TRUE
        if (*s == '0') RET_FALSE
    }
    char buf[8];
    size_t len = strlen(s);
    if (len >= sizeof(buf)) goto fail;
    for (size_t i = 0; i <= len; i++)
        buf[i] = (s[i] >= 'A' && s[i] <= 'Z') ? (char)(s[i] + 32) : s[i];
    if (strcmp(buf, "true")  == 0 || strcmp(buf, "yes") == 0 || strcmp(buf, "on")  == 0) RET_TRUE
    if (strcmp(buf, "false") == 0 || strcmp(buf, "no")  == 0 || strcmp(buf, "off") == 0) RET_FALSE
fail:
    cx->error = CLAG_ERR_INVALID_BOOL;
    return false;
#undef RET_FALSE
#undef RET_TRUE
}

// ---------------------------------------------------------------------------
// Apply a string token to a flag
// ---------------------------------------------------------------------------

static bool clag__apply_one(Clag_Context *cx, Clag *f, const char *raw)
{
#define RET_ERR(kind) { cx->error = kind; return false; }
    errno = 0;
    char *end;

    switch (f->type) {

    case CLAG_TYPE_BOOL: {
        bool v;
        if (!clag__parse_bool(cx, raw, &v)) return false;
        *(bool *)f->ref = v;
        return true;
    }

    case CLAG_TYPE_INT64: {
        long long v = strtoll(raw, &end, 0);
        if (end == raw || *end != '\0') RET_ERR(CLAG_ERR_INVALID_NUMBER);
        if (errno == ERANGE) {
            cx->error = (v < 0) ? CLAG_ERR_INT_UNDERFLOW : CLAG_ERR_INT_OVERFLOW;
            return false;
        }
        *(int64_t *)f->ref = (int64_t)v;
        return true;
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
        if (!clag__parse_size_str(cx, raw, &v)) return false;
        *(size_t *)f->ref = v;
        return true;
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
        *(char **)f->ref = (char *)raw;
        return true;
    }

    case CLAG_TYPE_LIST: {
        Clag_List *lst = (Clag_List *)f->ref;
        if (!f->list_delim) {
            clag_da_append(lst, (char *)raw);
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
                clag_da_append(lst, item);
            }
            if (!sep) break;
            p = sep + 1;
        }
        return true;
    }

    default:
        assert(0 && "unreachable");
        return false;
    }
#undef RET_ERR
}

static bool clag__run_validation(Clag_Context *cx, Clag *f)
{
#define RET_ERR(k) do { cx->error_flag_name = f->name; cx->error = (k); return false; } while(0)

    // 1. Range check
    if (f->range.active) {
        switch (f->type) {
        case CLAG_TYPE_INT64: {
            double v = (double)*(int64_t *)f->ref;
            if (v < f->range.lo || v > f->range.hi) RET_ERR(CLAG_ERR_RANGE);
            break;
        }
        case CLAG_TYPE_UINT64: {
            double v = (double)*(uint64_t *)f->ref;
            if (v < f->range.lo || v > f->range.hi) RET_ERR(CLAG_ERR_RANGE);
            break;
        }
        case CLAG_TYPE_DOUBLE: {
            double v = *(double *)f->ref;
            if (v < f->range.lo || v > f->range.hi) RET_ERR(CLAG_ERR_RANGE);
            break;
        }
        case CLAG_TYPE_FLOAT: {
            double v = (double)*(float *)f->ref;
            if (v < f->range.lo || v > f->range.hi) RET_ERR(CLAG_ERR_RANGE);
            break;
        }
        default: break;
        }
    }

    // 2. Enum / choices check (string flags only)
    if (f->choices && f->type == CLAG_TYPE_STR) {
        const char *val = *(char **)f->ref;
        bool found = false;
        for (const char **c = f->choices; *c; c++) {
            if (val && strcmp(val, *c) == 0) {
                found = true;
                break;
            }
        }
        if (!found) RET_ERR(CLAG_ERR_ENUM);
    }

    // 3. Custom validator (runs last so built-in checks already passed)
    if (f->validator) {
        f->validator_errbuf[0] = '\0';
        if (!f->validator(f->name, f->ref, f->validator_errbuf, sizeof(f->validator_errbuf))) {
            cx->error_flag_name = f->name;
            cx->error_detail    = f->validator_errbuf;
            cx->error           = CLAG_ERR_CUSTOM;
            return false;
        }
    }

    return true;
#undef RET_ERR
}

static bool clag__apply(Clag_Context *cx, Clag *f, const char *raw)
{
    if (f->deprecated)
        fprintf(stderr, "warning: flag --%s is deprecated: %s\n",
                f->name, f->depr_msg ? f->depr_msg : "");
    if (!clag__apply_one(cx, f, raw)) {
        if (!cx->error_flag_name) cx->error_flag_name = f->name;
        return false;
    }
    if (!clag__run_validation(cx, f)) return false;
    f->is_set = true;
    return true;
}

// ---------------------------------------------------------------------------
// Registration helpers
// ---------------------------------------------------------------------------

static Clag *clag__alloc(Clag_Context *cx, Clag_Type type, const char *name, char sc, const char *desc)
{
    assert(cx->flags_count < CLAG_CAP && "CLAG_CAP exceeded; raise #define CLAG_CAP");
    for (size_t i = 0; i < cx->flags_count; i++) {
        assert(strcmp(cx->flags[i].name, name) != 0    && "clag: duplicate flag name");
        assert(!(sc && cx->flags[i].short_char == sc)  && "clag: duplicate short flag character");
    }
    Clag *f = &cx->flags[cx->flags_count++];
    memset(f, 0, sizeof(*f));
    f->type        = type;
    f->name        = name;
    f->short_char  = sc;
    f->desc        = desc;
    f->group_idx   = cx->current_group;
    return f;
}

static Clag *clag__register(Clag_Context *cx, Clag_Type type, void *external_var, const char *name, char sc, const char *desc)
{
    Clag *f = clag__alloc(cx, type, name, sc, desc);
    if (external_var) {
        f->ref = external_var;
        f->ref_is_external = true;
    } else {
        f->ref = clag__val_ptr(&f->val, type);
        f->ref_is_external = false;
    }
    return f;
}

// ---------------------------------------------------------------------------
// Public registration API
// ---------------------------------------------------------------------------

#define CLAG_DEFINE_SCALAR(NAME, CTYPE, FIELD, ENUM_TYPE)                     \
CTYPE *clagc_##NAME(Clag_Context *cx, const char *name, char sc, CTYPE def, const char *desc) \
{                                                                             \
    Clag *f = clag__register(cx, ENUM_TYPE, NULL, name, sc, desc);            \
    f->def.FIELD = def;                                                       \
    *(CTYPE *)f->ref = def;                                                   \
    return (CTYPE *)f->ref;                                                   \
}                                                                             \
void clagc_##NAME##_var(Clag_Context *cx, CTYPE *v, const char *name, char sc, \
                       CTYPE def, const char *desc)                           \
{                                                                             \
    Clag *f = clag__register(cx, ENUM_TYPE, v, name, sc, desc);               \
    f->def.FIELD = def;                                                       \
    *v = def;                                                                 \
}                                                                             \
void clag_##NAME##_var(CTYPE *v, const char *name, char sc,                   \
                       CTYPE def, const char *desc)                           \
{ clagc_##NAME##_var(&clag_global_context, v, name, sc, def, desc); }                \
CTYPE *clag_##NAME(const char *name, char sc, CTYPE def, const char *desc)    \
{ return clagc_##NAME(&clag_global_context, name, sc, def, desc); }

CLAG_DEFINE_SCALAR(bool,   bool,     as_bool,   CLAG_TYPE_BOOL)
CLAG_DEFINE_SCALAR(float,  float,    as_float,  CLAG_TYPE_FLOAT)
CLAG_DEFINE_SCALAR(double, double,   as_double, CLAG_TYPE_DOUBLE)
CLAG_DEFINE_SCALAR(int64,  int64_t,  as_int64,  CLAG_TYPE_INT64)
CLAG_DEFINE_SCALAR(uint64, uint64_t, as_uint64, CLAG_TYPE_UINT64)
#undef CLAG_DEFINE_SCALAR

size_t *clagc_size(Clag_Context *cx, const char *name, char sc, const char *def_str, const char *desc)
{
    Clag *f = clag__register(cx, CLAG_TYPE_SIZE, NULL, name, sc, desc);
    f->def_str = def_str;
    size_t parsed = 0;
    if (def_str) {
        bool ok = clag__parse_size_str(cx, def_str, &parsed);
        assert(ok && "clag: invalid default size string");
    }
    f->def.as_size = parsed;
    *(size_t *)f->ref = parsed;
    return (size_t *)f->ref;
}

void clagc_size_var(Clag_Context *cx, size_t *v, const char *name, char sc, const char *def_str, const char *desc)
{
    Clag *f = clag__register(cx, CLAG_TYPE_SIZE, v, name, sc, desc);
    f->def_str = def_str;
    size_t parsed = 0;
    if (def_str) {
        bool ok = clag__parse_size_str(cx, def_str, &parsed);
        assert(ok && "clag: invalid default size string");
    }
    f->def.as_size = parsed; *v = parsed;
}

char **clagc_str(Clag_Context *cx, const char *name, char sc, const char *def, const char *desc)
{
    Clag *f = clag__register(cx, CLAG_TYPE_STR, NULL, name, sc, desc);
    f->def.as_str = (char *)def; *(char **)f->ref = (char *)def;
    return (char **)f->ref;
}

void clagc_str_var(Clag_Context *cx, char **v, const char *name, char sc, const char *def, const char *desc)
{
    Clag *f = clag__register(cx, CLAG_TYPE_STR, v, name, sc, desc);
    f->def.as_str = (char *)def; *v = (char *)def;
}

Clag_List *clagc_list(Clag_Context *cx, const char *name, char sc, char delim, const char *desc)
{
    Clag *f = clag__register(cx, CLAG_TYPE_LIST, NULL, name, sc, desc);
    f->list_delim = delim;
    memset(f->ref, 0, sizeof(Clag_List));
    return (Clag_List *)f->ref;
}

void clagc_list_var(Clag_Context *cx, Clag_List *v, const char *name, char sc, char delim, const char *desc)
{
    Clag *f = clag__register(cx, CLAG_TYPE_LIST, v, name, sc, desc);
    f->list_delim = delim;
    memset(v, 0, sizeof(Clag_List));
}

// ---------------------------------------------------------------------------
// Flag modifiers
// ---------------------------------------------------------------------------

#define CLAG__LOOKUP(cx, name)           \
    Clag *f = clag__find_long(cx, name); \
    assert(f && "clag modifier: unknown flag name")

void clagc_required  (Clag_Context *cx, const char *name) { CLAG__LOOKUP(cx, name); f->required = true; }
void clagc_hidden    (Clag_Context *cx, const char *name) { CLAG__LOOKUP(cx, name); f->hidden   = true; }
void clagc_deprecated(Clag_Context *cx, const char *name, const char *msg)
    { CLAG__LOOKUP(cx, name); f->deprecated = true; f->depr_msg = msg; }

#define CLAG_DEFINE_RANGE(NAME, CTYPE, ENUM)                  \
void clagc_range_##NAME(Clag_Context *cx, const char *name, CTYPE lo, CTYPE hi) \
{                                                                                  \
    CLAG__LOOKUP(cx, name);                                                        \
    assert(f->type == CLAG_TYPE_##ENUM && "clag_range_##NAME: flag is not ##NAME");  \
    f->range.active = true;                                                        \
    f->range.lo = (double)lo;                                                      \
    f->range.hi = (double)hi;                                                      \
} \
void clag_range_##NAME(const char *name, CTYPE lo, CTYPE hi) \
{ clagc_range_##NAME(&clag_global_context, name, lo, hi); }

CLAG_DEFINE_RANGE(int64, int64_t, INT64)
CLAG_DEFINE_RANGE(uint64, uint64_t, UINT64)
CLAG_DEFINE_RANGE(double, double, DOUBLE)
#undef CLAG_DEFINE_RANGE

void clagc__choices(Clag_Context *cx, const char *name, const char **choices)
{
    CLAG__LOOKUP(cx, name);
    assert(f->type == CLAG_TYPE_STR && "clag__choices: flag is not string");
    f->choices = choices;
}

void clag__choices(const char *name, const char **choices)
{
    clagc__choices(&clag_global_context, name, choices);
}

void clagc_validator(Clag_Context *cx, const char *name, ClagValidatorFn fn)
{
    CLAG__LOOKUP(cx, name);
    f->validator = fn;
}

#undef CLAG__LOOKUP

void clagc_alias(Clag_Context *cx, const char *name, const char *alias)
{
    assert(cx->aliases_count < CLAG_ALIAS_CAP && "CLAG_ALIAS_CAP exceeded");
    assert(clag__find_long(cx, name)   && "clag_alias: unknown primary flag name");
    assert(!clag__find_long(cx, alias) && "clag_alias: alias collides with a flag name");
    for (size_t i = 0; i < cx->aliases_count; i++)
        assert(strcmp(cx->aliases[i].alias, alias) != 0 && "clag_alias: duplicate alias");
    cx->aliases[cx->aliases_count].alias   = alias;
    cx->aliases[cx->aliases_count].primary = name;
    cx->aliases_count++;
}

void clagc_version(Clag_Context *cx, const char *version)
{
    cx->version_string = version;
}

// ---------------------------------------------------------------------------
// Constraint groups
// ---------------------------------------------------------------------------

void clag__mutex(Clag_Context *cx, const char *first, va_list ap)
{
    assert(cx->mutex_groups_count < CLAG_MUTEX_CAP && "CLAG_MUTEX_CAP exceeded");

    Clag_MutexGroup *g = &cx->mutex_groups[cx->mutex_groups_count++];
    g->count = 0;

    const char *name = first;

    while (name) {
        assert(g->count < CLAG_MUTEX_MEMBER_CAP && "CLAG_MUTEX_MEMBER_CAP exceeded");
        assert(clag__find_long(cx, name) && "clag_mutex: unknown flag name");

        g->members[g->count++] = name;

        name = va_arg(ap, const char *);
    }

    assert(g->count >= 2 && "clag_mutex: need at least 2 flags");
}

void clagc_mutex(Clag_Context *cx, const char *first, ...)
{
    va_list ap;
    va_start(ap, first);

    clag__mutex(cx, first, ap);

    va_end(ap);
}

void clagc_depends(Clag_Context *cx, const char *name, const char *requires)
{
    assert(cx->depends_count < CLAG_CAP && "too many depends entries");
    assert(clag__find_long(cx, name)     && "clag_depends: unknown flag name");
    assert(clag__find_long(cx, requires) && "clag_depends: unknown requires name");
    cx->depends[cx->depends_count].name     = name;
    cx->depends[cx->depends_count].requires = requires;
    cx->depends_count++;
}

// ---------------------------------------------------------------------------
// Help helpers
// ---------------------------------------------------------------------------

void clagc_usage(Clag_Context *cx, const char *synopsis) { cx->usage_synopsis = synopsis; }

void clagc_example(Clag_Context *cx, const char *text)
{
    assert(cx->examples_count < CLAG_EXAMPLE_CAP && "CLAG_EXAMPLE_CAP exceeded");
    cx->examples[cx->examples_count++] = text;
}

void clagc_group(Clag_Context *cx, const char *label)
{
    if (!label) {
        cx->current_group = -1;
        return;
    }
    assert(cx->groups_count < CLAG_GROUP_CAP && "CLAG_GROUP_CAP exceeded");
    Clag_GroupDef *gd = &cx->groups[cx->groups_count];
    gd->label           = label;
    gd->first_flag_idx  = cx->flags_count; // flags registered after this call
    cx->current_group = (int)cx->groups_count;
    cx->groups_count++;
}

// ---------------------------------------------------------------------------
// Parser
// ---------------------------------------------------------------------------

bool clagc_parse(Clag_Context *cx, int argc, char **argv)
{
    cx->program_name    = argc > 0 ? argv[0] : "";
    cx->error           = CLAG_OK;
    cx->error_flag_name = NULL;
    cx->error_detail    = NULL;

    cx->rest_argc = 0;
    cx->rest_argv = (char **)malloc(sizeof(char *) * (size_t)(argc < 1 ? 1 : argc));
    assert(cx->rest_argv && "clag: out of memory");

    for (int i = 1; i < argc; i++) {
        char *arg = argv[i];

        // "--" terminates flag parsing
        if (strcmp(arg, "--") == 0) {
            for (int j = i + 1; j < argc; j++)
                cx->rest_argv[cx->rest_argc++] = argv[j];
            break;
        }

        // positional argument
        if (arg[0] != '-' || arg[1] == '\0') {
            cx->rest_argv[cx->rest_argc++] = arg;
            continue;
        }

        // Strip one or two leading dashes
        char *name_start = arg + 1;
        bool is_long = (*name_start == '-');
        if (is_long) name_start++;

        // Split on '=' without mutating argv (argv may contain string literals)
        const char *eq  = strchr(name_start, '=');
        size_t name_len = eq ? (size_t)(eq - name_start) : strlen(name_start);
        // Priority:
        //   1. name_len==1 and char is a registered short -> short-flag path
        //   2. name_len>1  and first char is a non-bool short -> -oFILE value
        //   3. name_len>1  and all chars are bool shorts -> -abc cluster
        //   4. Everything else -> long-name lookup (covers -n where n is long
        //      name with no short char, and -verbose etc.)
        if (!is_long && !eq) {
            Clag *sf0 = clag__find_short(cx, name_start[0]);

            // Case 1: single short char
            if (name_len == 1 && sf0) {
                sf0->seen = true;
                if (sf0->type == CLAG_TYPE_BOOL) {
                    if (!clag__apply(cx, sf0, "true")) return false;
                } else {
                    if (i + 1 >= argc) {
                        cx->error = CLAG_ERR_NO_VALUE;
                        cx->error_flag_name = sf0->name;
                        return false;
                    }
                    if (!clag__apply(cx, sf0, argv[++i])) return false;
                }
                continue;
            }

            // Case 2: -oFILE style (non-bool short + rest as value)
            // E.g. "-out" with short 'o' and long flag "out": prefer long.
            if (name_len > 1 && sf0 && sf0->type != CLAG_TYPE_BOOL) {
                if (!clag__find_by_name(cx, name_start)) {
                    sf0->seen = true;
                    if (!clag__apply(cx, sf0, name_start + 1)) return false;
                    continue;
                }
            }

            // Case 3: bool cluster -abc
            if (name_len > 1 && sf0 && sf0->type == CLAG_TYPE_BOOL) {
                bool all_bool = true;
                for (size_t k = 0; k < name_len; k++) {
                    Clag *bf = clag__find_short(cx, name_start[k]);
                    if (!bf || bf->type != CLAG_TYPE_BOOL) { all_bool = false; break; }
                }
                if (all_bool) {
                    for (size_t k = 0; k < name_len; k++) {
                        Clag *bf = clag__find_short(cx, name_start[k]);
                        bf->seen = true;
                        if (!clag__apply(cx, bf, "true")) return false;
                    }
                    continue;
                }
            }
        }

        // Copy the flag name into a local buffer so we never write to argv.
        char name_buf[128];
        if (name_len >= sizeof(name_buf)) {
            cx->error = CLAG_ERR_UNKNOWN_FLAG;
            cx->error_flag_name = name_start;
            return false;
        }
        memcpy(name_buf, name_start, name_len);
        name_buf[name_len] = '\0';

        // Auto --help / -h
        if (strcmp(name_buf, "help") == 0 ||
            (!is_long && name_buf[0] == 'h' && name_buf[1] == '\0')) {
            clagc_print_help(cx, stdout);
            exit(0);
        }

        // --- Auto --version / -V ---
        // if (cx->version_string && 
        //         (strcmp(name_buf, "version") == 0 || 
        //         (!is_long && name_buf[0] == 'V' && name_buf[1] == '\0'))) {
        //     printf("%s %s\n", cx->program_name, cx->version_string);
        //     exit(0);
        // }

        // --no-<name>: boolean negation
        if (is_long && strncmp(name_buf, "no-", 3) == 0 && !eq) {
            const char *pos_name = name_buf + 3;
            Clag *nf = clag__find_by_name(cx, pos_name);
            if (nf && nf->type == CLAG_TYPE_BOOL) {
                nf->seen = true;
                if (!clag__apply(cx, nf, "false")) return false;
                continue;
            }
            // Fall through to unknown-flag error below if not found/not bool
        }

        // Long flag lookup
        Clag *f = clag__find_by_name(cx, name_buf);
        if (!f && !is_long && name_len == 1)
            f = clag__find_by_name(cx, name_buf);
        if (!f) {
            cx->error = CLAG_ERR_UNKNOWN_FLAG;
            cx->error_flag_name = name_start;
            return false;
        }
        f->seen = true;

        const char *inline_val = eq ? eq + 1 : NULL;
        if (f->type == CLAG_TYPE_BOOL && !inline_val) {
            if (!clag__apply(cx, f, "true")) return false;
            continue;
        }

        const char *val = inline_val;
        if (!val) {
            if (i + 1 >= argc) {
                cx->error = CLAG_ERR_NO_VALUE;
                cx->error_flag_name = f->name;
                return false;
            }
            val = argv[++i];
        }
        if (!clag__apply(cx, f, val)) return false;
    }

    // Check required flags
    for (size_t fi = 0; fi < cx->flags_count; fi++) {
        Clag *f = &cx->flags[fi];
        if (f->required && !f->is_set) {
            cx->error = CLAG_ERR_REQUIRED;
            cx->error_flag_name = f->name;
            return false;
        }
    }

    // Mutex group check
    for (size_t gi = 0; gi < cx->mutex_groups_count; gi++) {
        Clag_MutexGroup *g = &cx->mutex_groups[gi];
        const char *first_set = NULL;
        Clag *first_flag = NULL;

        for (size_t mi = 0; mi < g->count; mi++) {
            Clag *f = clag__find_by_name(cx, g->members[mi]);
            if (!f || !f->is_set) continue;

            if (first_flag && f != first_flag) {
                cx->error = CLAG_ERR_MUTEX;
                cx->error_flag_name = first_set;
                cx->error_detail    = f->name;
                return false;
            }
            first_flag = f;
            first_set  = f->name;
        }
    }

    // Dependency check
    for (size_t di = 0; di < cx->depends_count; di++) {
        Clag_Depend *d = &cx->depends[di];
        Clag *fa = clag__find_long(cx, d->name);
        Clag *fb = clag__find_long(cx, d->requires);
        if (fa && fa->is_set && fb && !fb->is_set) {
            cx->error = CLAG_ERR_DEPENDS;
            cx->error_flag_name = d->name;
            cx->error_detail    = d->requires;
            return false;
        }
    }

    return true;
}

// ---------------------------------------------------------------------------
// Accessors
// ---------------------------------------------------------------------------

int         clagc_rest_argc   (Clag_Context *cx) { return cx->rest_argc; }
char      **clagc_rest_argv   (Clag_Context *cx) { return cx->rest_argv; }
const char *clagc_program_name(Clag_Context *cx) { return cx->program_name; }

const char *clagc_name(Clag_Context *cx, void *val)
{
    for (size_t i = 0; i < cx->flags_count; i++)
        if (cx->flags[i].ref == val) return cx->flags[i].name;
    return NULL;
}

bool clagc_is_set (Clag_Context *cx, const char *name)
{ Clag *f = clag__find_by_name(cx, name); return f && f->is_set; }
bool clagc_was_seen(Clag_Context *cx, const char *name)
{ Clag *f = clag__find_by_name(cx, name); return f && f->seen; }

size_t clagc_count(Clag_Context *cx)
{
    size_t n = 0;
    for (size_t i = 0; i < cx->flags_count; i++)
        if (!cx->flags[i].hidden) n++;
    return n;
}

const char *clagc_flag_name_at(Clag_Context *cx, size_t idx)
{
    size_t n = 0;
    for (size_t i = 0; i < cx->flags_count; i++) {
        if (cx->flags[i].hidden) continue;
        if (n == idx) return cx->flags[i].name;
        n++;
    }
    return NULL;
}

const char *clagc_flag_desc_at(Clag_Context *cx, size_t idx)
{
    size_t n = 0;
    for (size_t i = 0; i < cx->flags_count; i++) {
        if (cx->flags[i].hidden) continue;
        if (n == idx) return cx->flags[i].desc;
        n++;
    }
    return NULL;
}

void clagc_reset(Clag_Context *cx)
{
    for (size_t i = 0; i < cx->flags_count; i++) {
        Clag *f = &cx->flags[i];
        if (f->type == CLAG_TYPE_LIST) {
            Clag_List *lst = (Clag_List *)f->ref;
            if (!lst || !lst->items) continue;

            for (size_t j = 0; j < lst->count; j++)
                free((void *)lst->items[j]);
            free(lst->items);
            lst->items = NULL;
            lst->count = 0;
            lst->capacity = 0;
        }
    }

    if (cx->rest_argv) {
        free(cx->rest_argv);
        cx->rest_argv = NULL;
    }
    cx->rest_argc = 0;
    memset(cx, 0, sizeof(*cx));
    cx->current_group = -1;
}
// ---------------------------------------------------------------------------
// Diagnostics
// ---------------------------------------------------------------------------

void clagc_print_error(Clag_Context *cx, FILE *s)
{
    if (cx->error == CLAG_OK) return;
    const char *prog = cx->program_name    ? cx->program_name    : "";
    const char *flag = cx->error_flag_name ? cx->error_flag_name : "(unknown)";

    switch (cx->error) {
    case CLAG_ERR_UNKNOWN_FLAG:
        fprintf(s, "%s: unknown flag: -%s\n", prog, flag);
        break;
    case CLAG_ERR_NO_VALUE:
        fprintf(s, "%s: flag needs an argument: --%s\n", prog, flag);
        break;
    case CLAG_ERR_INVALID_NUMBER:
        fprintf(s, "%s: invalid number for flag --%s\n", prog, flag);
        break;
    case CLAG_ERR_INT_OVERFLOW:
        fprintf(s, "%s: integer overflow for flag --%s\n", prog, flag);
        break;
    case CLAG_ERR_INT_UNDERFLOW:
        fprintf(s, "%s: integer underflow for flag --%s\n", prog, flag);
        break;
    case CLAG_ERR_FLOAT_OVERFLOW:
        fprintf(s, "%s: float overflow for flag --%s\n", prog, flag);
        break;
    case CLAG_ERR_DOUBLE_OVERFLOW:
        fprintf(s, "%s: double overflow for flag --%s\n", prog, flag);
        break;
    case CLAG_ERR_INVALID_SIZE_SUFFIX:
        fprintf(s, "%s: invalid size suffix for flag --%s (valid: K/M/G/T/P)\n",
                prog, flag);
        break;
    case CLAG_ERR_INVALID_BOOL:
        fprintf(s, "%s: invalid boolean for flag --%s"
                   " (valid: true/false/1/0/yes/no/on/off)\n", prog, flag);
        break;
    case CLAG_ERR_REQUIRED:
        fprintf(s, "%s: required flag not provided: --%s\n", prog, flag);
        break;
    case CLAG_ERR_MUTEX:
        fprintf(s, "%s: flags --%s and --%s are mutually exclusive\n",
                prog, flag,
                cx->error_detail ? cx->error_detail : "?");
        break;
    case CLAG_ERR_DEPENDS:
        fprintf(s, "%s: flag --%s requires --%s to also be set\n",
                prog, flag, cx->error_detail ? cx->error_detail : "?");
        break;
    case CLAG_ERR_ENUM: {
        Clag *ef = clag__find_long(cx, flag);
        fprintf(s, "%s: invalid value for --%s", prog, flag);
        if (ef && ef->choices) {
            fprintf(s, " (valid:");
            for (const char **c = ef->choices; *c; c++)
                fprintf(s, " %s", *c);
            fputc(')', s);
        }
        fputc('\n', s);
        break;
    }
    case CLAG_ERR_RANGE: {
        Clag *rf = clag__find_long(cx, flag);
        if (rf && rf->range.active)
            fprintf(s, "%s: value for --%s is out of range [%g, %g]\n",
                    prog, flag, rf->range.lo, rf->range.hi);
        else
            fprintf(s, "%s: value for --%s is out of range\n", prog, flag);
        break;
    }
    case CLAG_ERR_CUSTOM: {
        const char *msg = cx->error_detail;
        if (msg && *msg)
            fprintf(s, "%s: invalid value for --%s: %s\n", prog, flag, msg);
        else
            fprintf(s, "%s: invalid value for --%s (validation failed)\n", prog, flag);
        break;
    }
    default:
        fprintf(s, "%s: unknown error for flag --%s\n", prog, flag);
        break;
    }
}

// ---------------------------------------------------------------------------
// Help output
// ---------------------------------------------------------------------------

static void clag__print_default_cb(Clag__WriteFn out, void *ctx, const Clag *f)
{
    switch (f->type) {
    case CLAG_TYPE_BOOL:   out(ctx, f->def.as_bool ? "true" : "false"); break;
    case CLAG_TYPE_INT64:  clag__writef(out, ctx, "%" PRId64, f->def.as_int64); break;
    case CLAG_TYPE_UINT64: clag__writef(out, ctx, "%" PRIu64, f->def.as_uint64); break;
    case CLAG_TYPE_DOUBLE: clag__writef(out, ctx, "%g", f->def.as_double); break;
    case CLAG_TYPE_FLOAT:  clag__writef(out, ctx, "%g", (double)f->def.as_float); break;
    case CLAG_TYPE_SIZE:
        if (f->def_str) out(ctx, f->def_str);
        else clag__writef(out, ctx, "%zu", f->def.as_size);
        break;
    case CLAG_TYPE_STR:
        if (f->def.as_str) clag__writef(out, ctx, "\"%s\"", f->def.as_str);
        else out(ctx, "\"\"");
        break;
    case CLAG_TYPE_LIST:
        out(ctx, "[]");
        break;
    default: break;
    }
}

static const char *clag__type_name(Clag_Type t)
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


static void clag__wrap(
        FILE *s, const char *text, int indent_first, int indent_rest,
        const char **aliases, size_t alias_count, int nw, int width)
{
    const char *p = text;
    int line = 0;

    while (*p) {
        // Print the left-side prefix for this line
        if (line != 0) {
            // Which alias slot is this continuation line? (line 1 = first alias, etc.)
            size_t ai = (size_t)(line - 1);
            if (ai < alias_count) {
                // "      --<alias>" left-padded to match a no-short-char flag row
                // then padded out to indent_rest with spaces
                int written = fprintf(s, "      --%-*s", nw, aliases[ai]);
                // pad up to indent_rest
                for (int k = written; k < indent_rest; k++) fputc(' ', s);
            } else {
                fprintf(s, "%*s", indent_rest, "");
            }
        }

        // Word-wrap one line worth of text
        int avail = width - (line == 0 ? indent_first : indent_rest);
        if (avail < 1) avail = 1;

        int i = 0;
        int last_space = -1;
        while (p[i] && p[i] != '\n' && i < avail) {
            if (p[i] == ' ') last_space = i;
            i++;
        }

        int cut = i;
        if (p[i] && p[i] != '\n' && last_space > 0)
            cut = last_space;

        fwrite(p, 1, (size_t)cut, s);
        fputc('\n', s);

        p += cut;
        while (*p == ' ') p++;
        if (*p == '\n') p++;

        line++;
    }

    // Print any remaining aliases that didn't get a description line to sit on
    for (size_t ai = (size_t)(line > 0 ? line - 1 : 0); ai < alias_count; ai++) {
        int written = fprintf(s, "      --%-*s", nw, aliases[ai]);
        for (int k = written; k < indent_rest; k++) fputc(' ', s);
        fputc('\n', s);
    }
}

// Collect alias names for a primary flag (up to cap).
static size_t clag__flag_aliases(Clag_Context *cx, const char *primary, const char **out, size_t cap)
{
    size_t n = 0;
    for (size_t i = 0; i < cx->aliases_count && n < cap; i++)
        if (strcmp(cx->aliases[i].primary, primary) == 0)
            out[n++] = cx->aliases[i].alias;
    return n;
}

// Print one flag row.
static void clag__print_flag_row(Clag_Context *cx, FILE *s, const Clag *f, int nw, int prefix_w)
{
    if (f->hidden) return;

    if (f->short_char)
        fprintf(s, "  -%c, --%-*s", f->short_char, nw, f->name);
    else
        fprintf(s, "      --%-*s", nw, f->name);

    fprintf(s, "  %-8s  ", clag__type_name(f->type));

    char desc_buf[1024];
    int pos = 0, rem = (int)sizeof(desc_buf);

#define DAPP(...) do { \
    int _n = snprintf(desc_buf+pos,(size_t)rem,__VA_ARGS__); \
    if(_n>0){pos+=_n;rem-=_n;} } while(0)

    if (f->desc) DAPP("%s", f->desc);

    if (f->choices) {
        DAPP(" {");
        for (const char **c = f->choices; *c; c++) {
            if (c != f->choices) DAPP("|");
            DAPP(" %s ", *c);
        }
        DAPP("}");
    }

    if (f->range.active) DAPP(" [%g..%g]", f->range.lo, f->range.hi);

    if (f->type != CLAG_TYPE_LIST) {
        DAPP(" [default: ");
        Clag__BufCtx b = { desc_buf, &pos, &rem };
        clag__print_default_cb(clag__buf_write, &b, f);
        DAPP("]");
    }

    if (f->required)   DAPP(" (required)");
    if (f->deprecated) DAPP(" [DEPRECATED]");

#undef DAPP

    desc_buf[sizeof(desc_buf) - 1] = '\0';
    const char *anames[8];
    size_t an = clag__flag_aliases(cx, f->name, anames, 8);

    clag__wrap(s, desc_buf, prefix_w, prefix_w, anames, an, nw, CLAG_HELP_WIDTH);
}

void clagc_print_options(Clag_Context *cx, FILE *s)
{
    // Compute max name width
    size_t max_name = 4;
    for (size_t i = 0; i < cx->flags_count; i++) {
        if (cx->flags[i].hidden) continue;
        size_t n = strlen(cx->flags[i].name);
        if (n > max_name) max_name = n;
    }
    int nw       = (int)max_name;
    int prefix_w = 8 + nw + 2 + 8 + 2;

    // Print ungrouped flags first
    bool any_ungrouped = false;
    for (size_t i = 0; i < cx->flags_count; i++) {
        if (cx->flags[i].hidden) continue;
        if (cx->flags[i].group_idx >= 0) continue;
        any_ungrouped = true;
        clag__print_flag_row(cx, s, &cx->flags[i], nw, prefix_w);
    }

    // Print each named group
    for (size_t gi = 0; gi < cx->groups_count; gi++) {
        bool group_has_visible = false;
        for (size_t i = 0; i < cx->flags_count; i++) {
            if (cx->flags[i].hidden) continue;
            if (cx->flags[i].group_idx != (int)gi) continue;
            group_has_visible = true;
            break;
        }
        if (!group_has_visible) continue;

        if (any_ungrouped || gi > 0) fputc('\n', s);
        fprintf(s, "%s:\n", cx->groups[gi].label);

        for (size_t i = 0; i < cx->flags_count; i++) {
            if (cx->flags[i].hidden) continue;
            if (cx->flags[i].group_idx != (int)gi) continue;
            clag__print_flag_row(cx, s, &cx->flags[i], nw, prefix_w);
        }
    }
}

void clagc_print_help(Clag_Context *cx, FILE *s)
{
    const char *prog = cx->program_name   ? cx->program_name   : "program";
    const char *syn  = cx->usage_synopsis ? cx->usage_synopsis : "[options]";
    fprintf(s, "Usage: %s %s\n", prog, syn);
    if (clagc_count(cx) > 0) {
        fprintf(s, "\nOptions:\n");
        clagc_print_options(cx, s);
    }

    if (cx->examples_count > 0) {
        fprintf(s, "\nExamples:\n");
        for (size_t i = 0; i < cx->examples_count; i++)
            fprintf(s, "  %s\n", cx->examples[i]);
    }
}

// Global API wrappers.
// These functions forward directly to the context-aware clagc_* API
// using the internal global Clag_Context instance.

void clag_mutex(const char *first, ...)
{
    va_list ap;
    va_start(ap, first);

    clag__mutex(&clag_global_context, first, ap);

    va_end(ap);
}

void clag_required  (const char *name)
{ clagc_required(&clag_global_context, name); }

char **clag_str(const char *name, char sc, const char *def, const char *desc)
{ return clagc_str(&clag_global_context, name, sc, def, desc); }

size_t *clag_size(const char *name, char sc, const char *def, const char *desc)
{ return clagc_size(&clag_global_context, name, sc, def, desc); }

Clag_List *clag_list(const char *name, char sc, char delim, const char *desc)
{ return clagc_list(&clag_global_context, name, sc, delim, desc); }

void clag_size_var(size_t *v, const char *name, char sc, const char *def, const char *desc)
{ clagc_size_var(&clag_global_context, v, name, sc, def, desc); }

void clag_str_var(char **v, const char *name, char sc, const char *def, const char *desc)
{ clagc_str_var(&clag_global_context, v, name, sc, def, desc); }

void clag_list_var(Clag_List *v, const char *name, char sc, char delim, const char *desc)
{ clagc_list_var(&clag_global_context, v, name, sc, delim, desc); }

void clag_deprecated(const char *name, const char *msg)
{ clagc_deprecated(&clag_global_context, name, msg); }

void clag_hidden(const char *name)
{ clagc_hidden(&clag_global_context, name); }

void clag_validator(const char *name, ClagValidatorFn fn)
{ clagc_validator(&clag_global_context, name, fn); }

void clag_alias(const char *name, const char *alias)
{ clagc_alias(&clag_global_context, name, alias); }

void clag_depends(const char *name, const char *requires)
{ clagc_depends(&clag_global_context, name, requires); }

void clag_usage(const char *synopsis)
{ clagc_usage(&clag_global_context, synopsis); }

void clag_example(const char *text)
{ clagc_example(&clag_global_context, text); }

void clag_group(const char *label)
{ clagc_group(&clag_global_context, label); }

void clag_version(const char *version)
{ clagc_version(&clag_global_context, version); }

bool clag_parse(int argc, char **argv)
{ return clagc_parse(&clag_global_context, argc, argv); }

int clag_rest_argc(void)
{ return clagc_rest_argc(&clag_global_context); }

char **clag_rest_argv(void)
{ return clagc_rest_argv(&clag_global_context); }

const char *clag_program_name(void)
{ return clagc_program_name(&clag_global_context); }

void clag_print_error(FILE *stream)
{ clagc_print_error(&clag_global_context, stream); }

void clag_print_help(FILE *stream)
{ clagc_print_help(&clag_global_context, stream); }

void clag_print_options(FILE *stream)
{ clagc_print_options(&clag_global_context, stream); }

const char *clag_name(void *val)
{ return clagc_name(&clag_global_context, val); }

bool clag_is_set(const char *name)
{ return clagc_is_set(&clag_global_context, name); }

bool clag_was_seen(const char *name)
{ return clagc_was_seen(&clag_global_context, name); }

void clag_reset(void)
{ clagc_reset(&clag_global_context); }

size_t clag_count(void)
{ return clagc_count(&clag_global_context); }

const char *clag_flag_name_at(size_t i)
{ return clagc_flag_name_at(&clag_global_context, i); }

const char *clag_flag_desc_at(size_t i)
{ return clagc_flag_desc_at(&clag_global_context, i); }

#endif // CLAG_IMPLEMENTATION

/*
# Changelog

      3.0.1 (2026-04-12) Remove #define CLAG_IMPLEMENTATION before #ifdef CLAG_IMPLEMENTATION
      3.0.0 (2026-04-12)
         - Major refactor: introduce Clag_Context (remove global-only design)
         - Add clagc_* API for full context-based usage (multi-instance support)
         - Keep clag_* wrappers for backward compatibility (global context)
         - Rename core types for consistency:
             - ClagType    ->  Clag_Type
             - ClagValue   ->  Clag_Value
             - ClagList    ->  Clag_List
             - ClagError   ->  Clag_Error
             - ClagContext ->  Clag_Context
         - Make parser + validation fully context-scoped (no implicit globals)
         - Update alias/mutex/depends/range systems to be context-aware
         - Update help/print/reset APIs to operate on Clag_Context
         - Improve clag_choices macro for safer static storage usage

      2.3.1 (2026-04-11)
         - Fix clag_reset() to properly free internal allocations:
             - free list storage and individual list elements
             - free rest argument buffer
             - avoid memory leaks across repeated parses
         - Change clag_reset() semantics:
             - now frees internally owned memory instead of just resetting state
             - update documentation to reflect new behavior
         - Improve safety of reset logic:
             - handle NULL checks for list structures
             - ensure pointers are cleared after free
             - restore current_group invariant after full reset
         - Improve clag_choices macro:
             - replace compound literal with static storage
             - avoid lifetime issues with temporary arrays
             - make usage safer across different compilers and contexts
         - Correct clag_choices usage example

      2.3.0 (2026-04-11)
         - Add boolean negation support (--no-<flag>)
         - Add constraint system:
             - clag_mutex() for mutually exclusive flags
             - clag_depends() for flag dependencies
         - Add value validation:
             - clag_range_int64()
             - clag_range_uint64()
             - clag_range_double()
             - clag__choices() for enum-style string validation
             - clag_validator() for custom validation hooks
         - Add flag aliasing via clag_alias()
         - Add option grouping in help output via clag_group()
         - Add examples section in help via clag_example()
         - Add automatic --version / -V support via clag_version()
         - Add public flag iteration API:
             - clag_count()
             - clag_flag_name_at()
             - clag_flag_desc_at()
         - Add clag_reset() to clear parser state (useful for testing)
         - Add new error types:
             - CLAG_ERR_MUTEX
             - CLAG_ERR_DEPENDS
             - CLAG_ERR_ENUM
             - CLAG_ERR_RANGE
             - CLAG_ERR_CUSTOM
         - Improve error messages:
             - consistent "--flag" formatting
             - show valid choices for enums
             - show range bounds for numeric constraints
             - clearer dependency and mutex diagnostics
         - Improve help output:
             - show aliases inline within wrapped descriptions (ffmpeg-style)
             - support alias rendering as continuation lines aligned to description column
             - show enum choices and value ranges
             - support grouped options with headers
             - add examples section
             - improved wrapping with prefix-aware continuation (clag__wrap_ex)
         - Add configurable limits:
             - CLAG_MUTEX_CAP, CLAG_MUTEX_MEMBER_CAP
             - CLAG_EXAMPLE_CAP, CLAG_GROUP_CAP, CLAG_ALIAS_CAP
             - CLAG_VALIDATOR_ERRBUF_SIZE
         - Improve parsing behavior:
             - non-flag arguments are collected without stopping parsing
             - alias-aware lookup during parsing
         - Internal refactor:
             - rename g_clag to clag_global
             - introduce clag__run_validation()
             - unify scalar definitions via macros
             - separate alias-aware lookup path
             - cleaner help rendering pipeline
             - introduce prefix-aware wrapping (clag__wrap_ex)

      2.2.0 (2026-04-08)
         - Add clag_was_seen(const char *name)
         - Include stdarg.h

      2.1.0 (2026-04-04)
         - Remove fmemopen (Windows compatibility)
         - Add callback-based writer for formatting
         - Improve help formatting
         - Fix buffer handling in description builder

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
