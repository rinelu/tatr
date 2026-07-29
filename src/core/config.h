#ifndef CONFIG_H_
#define CONFIG_H_

#include "astring.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "error.h"

// ~/.config/tatr/config (resolved at runtime via config_global_path())
const char *config_global_path(void);

typedef enum {
    CFG_STRING = 0,
    CFG_BOOL,       // true, false, 1, 0
    CFG_INT,        // a base-10 integer (may be negative)
    CFG_ENUM,       // exactly one of enum_values
} Config_Type;

typedef struct {
    const char *key;
    const char *desc;
    const char *default_val;    // NULL = no default
    Config_Type  type;
    const char *const *enum_values; // NULL-terminated. Only for CFG_ENUM
} Config_Key_Def;

extern const Config_Key_Def CONFIG_KEYS[];
extern const size_t         CONFIG_KEYS_COUNT;

// Returns the definition for `key`, or NULL if unknown.
const Config_Key_Def *config_key_def(const char *key);

// Whether `val` satisfies `def` type.
bool config_validate(const Config_Key_Def *def, const char *val);

// Human-readable description of what config_validate(def, ...) accepts,
// e.g. "one of: low, normal, high, critical" or "true or false".
// Used to build a helpful error message; never returns NULL.
const char *config_expected_desc(const Config_Key_Def *def);

typedef struct {
    String_View key;
    String_View val;
} Config_Entry;

typedef struct {
    struct {
        Config_Entry *items;
        size_t count;
        size_t capacity;
    } entries;

    char  *_buf; // owns string data
    char  *path; // heap-allocated path string
} Config_Store;

typedef struct {
    Config_Store local;
    Config_Store global;
} Config;

// Load both scopes.
// Missing files are silently skipped (not an error).
void config_load(Config *c);

// Get the resolved value for `key` (local wins over global).
// Returns NULL if not set in either scope.
const char *config_get(const Config *c, const char *key);

// Get with fallback to default_val if key is unset.
// Never returns NULL.
const char *config_get_or_default(const Config *c, const char *key);

// Falls back to the schema default (or 0/false if there is none)
// for an unset or CFG_INVALID for this accessor key.
bool    config_get_bool(const Config *c, const char *key);
int64_t config_get_int(const Config *c, const char *key);

const char *store_get_source(const Config *c, const char *key);

// Set a key in the given scope file. Validates `val` against the key
// schema before writing anything.
//   TATR_OK              - written successfully
//   TATR_ERR_INVALID_ARG - unknown key, or val doesn't satisfy its type
//   TATR_ERR_IO          - the write itself failed
Tatr_Error config_set(const char *path, const char *key, const char *val);

// Remove a key from the given scope file.
// Returns false if the key was not present or on I/O error.
bool config_unset(const char *path, const char *key);

// Free all heap memory owned by a Config.
void config_free(Config *c);

bool config_write_global_default(void);

#endif // CONFIG_H_
